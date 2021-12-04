#include "bindings.hlsli"
#include "brdf.hlsli"

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
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
Texture3D<float4> g_Textures3D[65536] : REGISTER_SRV(0, 1, 0);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 2, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float exposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	
	float4 albedoMetalness = g_Textures[g_PushConsts.albedoTextureIndex].Load(uint3(input.position.xy, 0));
	
	float gtao = albedoMetalness.a != 1.0f ? g_Textures[g_PushConsts.gtaoTextureIndex].Load(uint3(input.position.xy, 0)).x : 0.0f;
	
	float intensity = 1.0f;
	float3 result = Diffuse_Lambert(albedoMetalness.rgb) * intensity * (1.0f - albedoMetalness.a) * gtao * exposure;
	
	// volumetric fog
	{
		const float depth = g_Textures[g_PushConsts.depthBufferIndex].Load(int3(input.position.xy, 0)).x;
		const float linearDepth = rcp(g_PushConsts.depthUnprojectParams.x * depth + g_PushConsts.depthUnprojectParams.y);
		float d = (log2(max(0, linearDepth * rcp(g_PushConsts.volumetricFogNear))) * rcp(log2(g_PushConsts.volumetricFogFar / g_PushConsts.volumetricFogNear)));
	
		d = max(0.0, d - g_PushConsts.volumetricFogTexelSize.z * 1.5f);
		
		float4 fog = g_Textures3D[g_PushConsts.volumetricFogTextureIndex].SampleLevel(g_LinearClampSampler, float3(input.uv, d), 0.0f);
		
		result = result * fog.a + fog.rgb;
	}
	
	return float4(result, 0.0f);
}