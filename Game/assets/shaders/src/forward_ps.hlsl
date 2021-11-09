#include "bindings.hlsli"
#include "shadingModel.hlsli"
#include "packing.hlsli"

#ifndef SKINNED
#define SKINNED 0
#endif

struct PSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	float3 worldSpacePosition : WORLD_SPACE_POS;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normalRoughness : SV_Target1;
	float4 albedoMetalness : SV_Target2;
	float2 velocity : SV_Target3;
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
	float3 cameraPosition;
	uint skinningMatricesBufferIndex;
};

struct DrawConstants
{
	float4x4 modelMatrix;
#if SKINNED
	uint skinningMatricesOffset;
#endif
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
PUSH_CONSTS(DrawConstants, g_DrawConstants);

PSOutput main(PSInput input)
{
	float3 lightDir = normalize(float3(1.0f, 1.0f, 1.0f));
	float3 lightIntensity = 1.0f;
	
	float3 V = normalize(g_PassConstants.cameraPosition - input.worldSpacePosition);
	float3 albedo = float3(input.texCoord, 0.0f); // TODO
	float3 N = normalize(input.normal);
	float metalness = 0.0f;
	float roughness = 0.2f;
	const float3 F0 = lerp(0.04f, albedo, metalness);
	
	float3 result = 0.0f;
	
	// directional light
	{
		result += Default_Lit(albedo, F0, lightIntensity, N, V, lightDir, roughness, metalness);
	}
	
	// ambient light
	{
		result += 0.1f * albedo;
	}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, 1.0f);
	output.normalRoughness = float4(encodeOctahedron24(N), roughness);
	output.albedoMetalness = float4(albedo, metalness);
	
	return output;
}