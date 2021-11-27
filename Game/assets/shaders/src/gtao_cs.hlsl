#include "bindings.hlsli"
#include "packing.hlsli"

#ifndef PI
#define PI (3.14159265359)
#endif // PI
					
struct GTAOConstants
{
	float4x4 viewMatrix;
	float4x4 invProjectionMatrix;
	uint2 resolution;
	float2 texelSize;
	float radiusScale;
	float falloffScale;
	float falloffBias;
	float maxTexelRadius;
	int sampleCount;
	uint frame;
	uint depthTextureIndex;
	uint normalTextureIndex;
	uint resultTextureIndex;
};

ConstantBuffer<GTAOConstants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 1);
SamplerState g_PointClampSampler : REGISTER_SAMPLER(0, 0, 2);


float3 getViewSpacePos(float2 uv)
{
	float depth = g_Textures[g_Constants.depthTextureIndex].SampleLevel(g_PointClampSampler, uv, 0.0f).x;
	float4 viewSpacePosition = mul(g_Constants.invProjectionMatrix, float4(uv * float2(2.0f, -2.0f) - float2(1.0f, -1.0f), depth, 1.0f));
	return viewSpacePosition.xyz / viewSpacePosition.w;
}

float3 minDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (dot(V1, V1) < dot(V2, V2)) ? V1 : V2;
}

float fastSqrt(float x)
{
	// [Drobot2014a] Low Level Optimizations for GCN
	return asfloat(0x1FBD1DF5 + (asint(x) >> 1));
}

float fastAcos(float x)
{
	// [Eberly2014] GPGPU Programming for Games and Science
	float res = -0.156583f * abs(x) + PI / 2.0f;
	res *= fastSqrt(1.0 - abs(x));
	return x >= 0 ? res : PI - res;
}

// x = spatial direction / y = temporal direction / z = spatial offset / w = temporal offset
float4 getNoise(int2 coord, int frame)
{
	float4 noise;
	
	noise.x = (1.0f / 16.0f) * ((((coord.x + coord.y) & 0x3 ) << 2) + (coord.x & 0x3));
	noise.z = (1.0f / 4.0f) * ((coord.y - coord.x) & 0x3);

	const float rotations[] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
	noise.y = rotations[frame % 6] * (1.0f / 360.0f);
	
	const float offsets[] = { 0.0f, 0.5f, 0.25f, 0.75f };
	noise.w = offsets[(frame / 6 ) % 4];
	
	return noise;
}

float getHorizonCosAngle(float3 centerPos, float3 samplePos, float3 V)
{
	float3 sampleDir = samplePos - centerPos;
	float dist = length(sampleDir);
	sampleDir = normalize(sampleDir);
	
	float falloff = saturate(dist * g_Constants.falloffScale + g_Constants.falloffBias);
	return lerp(dot(sampleDir, V), -1.0f, falloff);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_Constants.resolution))
	{
		return;
	}

	float2 texCoord = (threadID.xy + 0.5f) * g_Constants.texelSize;
	float3 centerPos = getViewSpacePos(texCoord);
	float3 V = normalize(-centerPos);
	
	const float3 N = mul((float3x3)g_Constants.viewMatrix, decodeOctahedron24(g_Textures[g_Constants.normalTextureIndex].Load(int3(threadID.xy, 0)).xyz));
	
	//// Sample neighboring pixels
	//const float3 Pr = getViewSpacePos((float2(threadID.xy + 0.5) + float2(1.0, 0.0)) * g_Constants.texelSize);
	//const float3 Pl = getViewSpacePos((float2(threadID.xy + 0.5) + float2(-1.0, 0.0)) * g_Constants.texelSize);
	//const float3 Pb = getViewSpacePos((float2(threadID.xy + 0.5) + float2(0.0, 1.0)) * g_Constants.texelSize);
	//const float3 Pt = getViewSpacePos((float2(threadID.xy + 0.5) + float2(0.0, -1.0)) * g_Constants.texelSize);
	//
	//// Calculate tangent basis vectors using the minimum difference
	//const float3 dPdu = minDiff(centerPos, Pr, Pl);
	//const float3 dPdv = minDiff(centerPos, Pt, Pb);
	//
	//const float3 N = normalize(cross(dPdu, dPdv));
	
	float scaling = g_Constants.radiusScale / -centerPos.z;
	scaling = min(scaling, g_Constants.texelSize.x * g_Constants.maxTexelRadius);
	int sampleCount = min(ceil(scaling * g_Constants.resolution.x), g_Constants.sampleCount);
	
	float visibility = 1.0f;
	
	if (sampleCount > 0)
	{
		visibility = 0.0f;
		float4 noise = getNoise(threadID.xy, g_Constants.frame);

		float theta = (noise.x + noise.y) * PI;
		float jitter = noise.z + noise.w;
		float3 dir = 0.0f;
		sincos(theta, dir.y, dir.x);
		
		float3 orthoDir = dir - dot(dir, V) * V;
		float3 axis = cross(dir, V);
		float3 projNormal = N - axis * dot(N, axis);
		float projNormalLen = length(projNormal);
		
		float sgnN = sign(dot(orthoDir, projNormal));
		float cosN = saturate(dot(projNormal, V) / projNormalLen);
		float n = sgnN * fastAcos(cosN);
		float sinN = sin(n);
		
		[unroll]
		for (int side = 0; side < 2; ++side)
		{
			float horizonCos = -1.0f;
			
			for (int sample = 0; sample < sampleCount; ++sample)
			{
				float s = (sample + jitter) / (float)sampleCount;
				s *= s;
				float2 sampleTexCoord = texCoord + (-1.0f + 2.0f * side) * s * scaling * float2(dir.x, -dir.y);
				float3 samplePos = getViewSpacePos(sampleTexCoord);
				float sampleHorizonCos = getHorizonCosAngle(centerPos, samplePos, V);
				
				const float beta = 0.05f;
				const float minDistToMax = 0.1f;
				horizonCos = ((horizonCos - sampleHorizonCos) > minDistToMax) ? max(horizonCos - beta, -1.0f) : max(sampleHorizonCos, horizonCos);
			}	
			
			float h = n + clamp((-1.0f + 2.0f * side) * fastAcos(horizonCos) - n, -PI / 2, PI / 2);
			visibility += projNormalLen * (cosN + 2.0f * h * sinN - cos(2.0f * h - n)) * 0.25f;
		}
	}
	
	visibility = saturate(visibility);

	g_RWTextures[g_Constants.resultTextureIndex][threadID.xy] = float4(visibility, -centerPos.z, 0.0f, 0.0f);
}