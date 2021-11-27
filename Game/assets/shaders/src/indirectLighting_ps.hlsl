#include "bindings.hlsli"
#include "brdf.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

struct PushConsts
{
	uint exposureBufferIndex;
	uint albedoTextureIndex;
	uint gtaoTextureIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 1, 0);
PUSH_CONSTS(PushConsts, g_PushConsts);

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float exposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	
	float4 albedoMetalness = g_Textures[g_PushConsts.albedoTextureIndex].Load(uint3(input.position.xy, 0));
	
	float gtao = albedoMetalness.a != 1.0f ? g_Textures[g_PushConsts.gtaoTextureIndex].Load(uint3(input.position.xy, 0)).x : 0.0f;
	
	float intensity = 1.0f;
	float3 result = Diffuse_Lambert(albedoMetalness.rgb) * intensity * (1.0f - albedoMetalness.a) * gtao * exposure;
	
	return float4(result, 0.0f);
}