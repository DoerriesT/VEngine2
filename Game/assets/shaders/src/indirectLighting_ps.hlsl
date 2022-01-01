#include "bindings.hlsli"
#include "brdf.hlsli"
#include "srgb.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD;
};

struct PushConsts
{
	float3 volumetricFogTexelSize;
	float volumetricFogNear;
	float2 depthUnprojectParams;
	float volumetricFogFar;
	uint exposureBufferIndex;
	uint albedoTextureIndex;
	uint gtaoTextureIndex;
	uint depthBufferIndex;
	uint volumetricFogTextureIndex;
	uint blueNoiseTextureIndex;
	uint frame;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
Texture3D<float4> g_Textures3D[65536] : REGISTER_SRV(0, 1, 0);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 2, 0);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 3, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 inverseSimpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	return color / max(1.0f - luma, 1e-6f);
}

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float exposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	
	float4 albedoMetalness = g_Textures[g_PushConsts.albedoTextureIndex].Load(uint3(input.position.xy, 0));
	
	float gtao = g_Textures[g_PushConsts.gtaoTextureIndex].Load(uint3(input.position.xy, 0)).x;
	
	float intensity = 1.0f;
	float3 result = Diffuse_Lambert(albedoMetalness.rgb) * intensity * (1.0f - albedoMetalness.a) * gtao * exposure;
	
	// volumetric fog
	{
		float4 noise = g_ArrayTextures[g_PushConsts.blueNoiseTextureIndex].Load(int4(int2(input.position.xy) & 63, g_PushConsts.frame & 63, 0));
		noise = (noise * 2.0f - 1.0f) * 1.5f;
		
		const float depth = g_Textures[g_PushConsts.depthBufferIndex].Load(int3(input.position.xy, 0)).x;
		const float linearDepth = rcp(g_PushConsts.depthUnprojectParams.x * depth + g_PushConsts.depthUnprojectParams.y);
		float d = (log2(max(0, linearDepth * rcp(g_PushConsts.volumetricFogNear))) * rcp(log2(g_PushConsts.volumetricFogFar / g_PushConsts.volumetricFogNear)));
	
		d = max(0.0f, d - g_PushConsts.volumetricFogTexelSize.z * 1.5f);
		
		float4 fog = 0.0f;
		for (int i = 0; i < 4; ++i)
		{
			float3 tc = float3(input.uv, d) + noise.xyz * float3(1.0f, 1.0f, 0.5f) * g_PushConsts.volumetricFogTexelSize;
			fog += g_Textures3D[g_PushConsts.volumetricFogTextureIndex].SampleLevel(g_LinearClampSampler, tc, 0.0f) / 4.0f;
			noise = noise.yzwx;
		}
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		
		result = result * fog.a + fog.rgb;
	}
	
	return float4(result, 0.0f);
}