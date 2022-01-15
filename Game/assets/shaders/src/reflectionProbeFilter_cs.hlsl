#include "bindings.hlsli"
#include "brdf.hlsli"

struct PushConsts
{
	uint resolution;
	float texelSize;
	float mip0Resolution;
	float roughness;
	float mipCount;
	uint copyOnly;
	uint inputTextureIndex;
	uint resultTextureIndex;
};

TextureCube<float4> g_CubeTextures[65536] : REGISTER_SRV(0, 0, 0);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 1, 0);
RWTexture2DArray<float4> g_RWTextures[65536] : REGISTER_UAV(1, 2, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

// based on information from "Physically Based Shading at Disney" by Brent Burley
float3 importanceSampleGGX(float2 noise, float3 N, float a2)
{
	float phi = 2.0f * PI * noise.x;
	float cosTheta = sqrt((1.0f - noise.y) / (1.0f + (a2 - 1.0f) * noise.y + 1e-5f) + 1e-5f);
	cosTheta = saturate(cosTheta);
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta + 1e-5f);
	
	float sinPhi;
	float cosPhi;
	sincos(phi, sinPhi, cosPhi);
	float3 H = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
	
	// transform H from tangent-space to world-space
	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);
	
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits) 
{
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i)/float(N), RadicalInverse_VdC(i));
}

float3 integrateCubeLDOnly(float3 N, float roughness)
{
	const uint k_numSamples = 32;
	
	const float a = roughness * roughness;
	const float a2 = a * a;
	
	// solid angle of cubemap pixel
	const float rcpOmegaP = 1.0f / (4.0f * PI / (6.0f * g_PushConsts.mip0Resolution * g_PushConsts.mip0Resolution));
	
	float3 sum = 0.0f;
	float weightSum = 0.0f;
	
	for (int i = 0; i < k_numSamples; ++i)
	{
		const float2 eta = Hammersley(i, k_numSamples);
		const float3 H = importanceSampleGGX(eta, N, a2);
		const float3 V = N;
		const float3 L = reflect(-V, H);
		const float NdotL = dot(N, L);
		
		if (NdotL > 0.0f)
		{
			const float NdotH = saturate(dot(N, H));
			const float LdotH = saturate(dot(L, H));
			const float pdf = (D_GGX(NdotH, a2) / PI) * NdotH / (4.0f * LdotH);
			
			// solid angle of sample
			const float omegaS = 1.0f / (k_numSamples * pdf);
			
			const float mipLevel = clamp(0.5f * log2(omegaS * rcpOmegaP), 0.0f, g_PushConsts.mipCount);
			const float3 Li = g_CubeTextures[g_PushConsts.inputTextureIndex].SampleLevel(g_LinearClampSampler, L, mipLevel).rgb;
			
			sum += Li * NdotL;
			weightSum += NdotL;
		}
	}
	
	return sum / weightSum;
}

float3 getDir(float2 uv, int face)
{
	uv = uv * 2.0f - 1.0f;
	uv.y = -uv.y;

	switch (face)
	{
	case 0:
		return float3(1.0f, uv.y, -uv.x);
	case 1:
		return float3(-1.0f, uv.y, uv.x);
	case 2:
		return float3(uv.x, 1.0f, -uv.y);
	case 3:
		return float3(uv.x, -1.0f, uv.y);
	case 4:
		return float3(uv.x, uv.y, 1.0f);
	default:
		return float3(-uv.x, uv.y, -1.0f);
	}
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_PushConsts.resolution))
	{
		return;
	}
	
	// just copy the first mip, it is probably already too low resolution for good mirror reflections
	if (g_PushConsts.copyOnly != 0)
	{
		g_RWTextures[g_PushConsts.resultTextureIndex][threadID] = g_ArrayTextures[g_PushConsts.inputTextureIndex].Load(uint4(threadID, 0));
		return;
	}
	
	const float2 uv = (threadID.xy + 0.5f) * g_PushConsts.texelSize;
	const float3 N = getDir(uv, threadID.z);
	
	g_RWTextures[g_PushConsts.resultTextureIndex][threadID] = float4(integrateCubeLDOnly(N, g_PushConsts.roughness), 1.0f);
}