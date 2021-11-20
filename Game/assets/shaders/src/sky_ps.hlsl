#include "bindings.hlsli"
#include "srgb.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float4 ray : RAY;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normalRoughness : SV_Target1;
	float4 albedoMetalness : SV_Target2;
	float2 velocity : SV_Target3;
};

struct PushConsts
{
	float4x4 invModelViewProjection;
	uint exposureBufferIndex;
};

ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 0, 0);
PUSH_CONSTS(PushConsts, g_PushConsts);

[earlydepthstencil]
PSOutput main(PSInput input)
{
	float3 skyColor = accurateSRGBToLinear(float3(0.529f, 0.808f, 0.922f));
	float3 groundColor = accurateSRGBToLinear(float3(240.0f, 216.0f, 195.0f) / 255.0f);
	
	float3 ray = normalize(input.ray.xyz);
	
	PSOutput output = (PSOutput)0;
	
	output.color = float4(lerp(groundColor, skyColor, ray.y * 0.5f + 0.5f));//float4(0.2f, 0.2f, 1.0f, 1.0f);
	output.normalRoughness = 0.0f;
	output.albedoMetalness = 0.0f;
	output.velocity = 0.0f;
	
	// apply pre-exposure
	output.color.rgb *= asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	
	return output;
}