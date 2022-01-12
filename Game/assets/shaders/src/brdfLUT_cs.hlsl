#include "bindings.hlsli"
#include "brdf.hlsli"

struct PushConsts
{
	uint resolution;
	float texelSize;
	uint resultTextureIndex;
};

RWTexture2D<float2> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

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

float2 integrateDFGOnly(float NdotV, float roughness)
{
	const uint k_numSamples = 1024;
	
	float a = roughness * roughness;
	float a2 = a * a;
	
	float3 V;
	V.x = sqrt(1.0f - NdotV * NdotV); // sin
	V.y = 0.0f;
	V.z = NdotV; // cos;
	
	float2 sum = 0.0f;
	float weightSum = 0.0f;
	
	for (uint i = 0; i < k_numSamples; ++i)
	{
		const float2 eta = Hammersley(i, k_numSamples);
		const float3 H = importanceSampleGGX(eta, float3(0.0f, 0.0f, 1.0f), a2);
		const float3 L = reflect(-V, H);
		
		const float NdotL = saturate(L.z);
		const float NdotH = saturate(H.z);
		const float VdotH = saturate(dot(V, H));
		
		if (NdotL > 0.0f)
		{
			const float Vis = V_SmithGGXCorrelated(NdotV, NdotL, a2);
			const float NdotL_Vis_PDF = NdotL * Vis * 4.0f * VdotH / NdotH;
			const float Fc = pow5(1.0f - VdotH);
			sum.x += (1.0f - Fc) * NdotL_Vis_PDF;
			sum.y += Fc * NdotL_Vis_PDF;
			weightSum += 1.0f;
		}
	}
	
	return sum / weightSum;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_PushConsts.resolution))
	{
		return;
	}
	
	float NdotV = (threadID.x + 0.5f) * g_PushConsts.texelSize;
	float roughness = (threadID.y + 0.5f) * g_PushConsts.texelSize;
	
	g_RWTextures[g_PushConsts.resultTextureIndex][threadID.xy] = integrateDFGOnly(NdotV, roughness);
}