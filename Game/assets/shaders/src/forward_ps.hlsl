#include "bindings.hlsli"
#include "shadingModel.hlsli"
#include "packing.hlsli"
#include "srgb.hlsli"
#include "material.hlsli"
#include "lights.hlsli"
#include "picking.hlsli"

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
	uint materialBufferIndex;
	uint directionalLightBufferIndex;
	uint directionalLightCount;
	uint directionalLightShadowedBufferIndex;
	uint directionalLightShadowedCount;
	uint exposureBufferIndex;
	uint pickingBufferIndex;
	uint pickingPosX;
	uint pickingPosY;
};

struct DrawConstants
{
	float4x4 modelMatrix;
	uint materialIndex;
	uint entityID;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<Material> g_Materials[65536] : REGISTER_SRV(4, 2, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLights[65536] : REGISTER_SRV(4, 3, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed[65536] : REGISTER_SRV(4, 4, 1);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 5, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 6, 1);
RWByteAddressBuffer g_RWByteAddressBuffers[65536] : REGISTER_UAV(5, 7, 1);
SamplerState g_AnisoRepeatSampler : REGISTER_SAMPLER(0, 0, 2);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(1, 0, 2);
PUSH_CONSTS(DrawConstants, g_DrawConstants);

float3 sampleCascadedShadowMaps(float3 posWS, float3 N, DirectionalLight light)
{	
	float4 shadowCoord = 0.0f;
	uint cascadeIndex = 0;
	bool foundCascade = false;
	for (; cascadeIndex < light.cascadeCount; ++cascadeIndex)
	{
		shadowCoord = mul(light.shadowMatrices[cascadeIndex], float4(posWS + N * 0.1f, 1.0f));
		if (all(abs(shadowCoord.xy) < 1.0f))
		{
			foundCascade = true;
			break;
		}
	}
	
	if (foundCascade)
	{
		return g_ArrayTextures[light.shadowTextureHandle].SampleCmpLevelZero(g_ShadowSampler, float3(shadowCoord.xy * float2(0.5f, -0.5f) + 0.5f, cascadeIndex), shadowCoord.z).x;
	}
	else
	{
		return 1.0f;
	}
}

[earlydepthstencil]
PSOutput main(PSInput input)
{
	updatePickingBuffer(
		g_RWByteAddressBuffers[g_PassConstants.pickingBufferIndex], 
		(uint2)input.position.xy, 
		uint2(g_PassConstants.pickingPosX, g_PassConstants.pickingPosY),
		g_DrawConstants.entityID, 
		input.position.z
	);
	
	Material material = g_Materials[g_PassConstants.materialBufferIndex][g_DrawConstants.materialIndex];
	
	// albedo
	float3 albedo;
	{
		albedo = unpackUnorm4x8(material.albedo).rgb;
		if (material.albedoTextureHandle != 0)
		{
			albedo = g_Textures[material.albedoTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord).rgb;
		}
		albedo = accurateSRGBToLinear(albedo);
	}
	
	// normal
	float3 N = normalize(input.normal);
	float3 vertexNormal = N;
	{
		if (material.normalTextureHandle != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[material.normalTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);

			float3 bitangent = cross(input.normal, input.tangent.xyz) * input.tangent.w;
			N = tangentSpaceNormal.x * input.tangent.xyz + tangentSpaceNormal.y * bitangent + tangentSpaceNormal.z * input.normal;
			
			N = normalize(N);
			N = any(isnan(N)) ? normalize(input.normal) : N;
		}
	}
	
	// metalness
	float metalness;
	{
		metalness = material.metalness;
		if (material.metalnessTextureHandle != 0)
		{
			metalness = g_Textures[material.metalnessTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord).z;
		}
	}
	
	// roughness
	float roughness;
	{
		roughness = material.roughness;
		if (material.roughnessTextureHandle != 0)
		{
			roughness = g_Textures[material.roughnessTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord).y;
		}
	}
	
	float3 V = normalize(g_PassConstants.cameraPosition - input.worldSpacePosition);
	const float3 F0 = lerp(0.04f, albedo, metalness);
	
	const float exposure = asfloat(g_ByteAddressBuffers[g_PassConstants.exposureBufferIndex].Load(0));
	
	float3 result = 0.0f;
	
	// directional lights
	{
		for (uint i = 0; i < g_PassConstants.directionalLightCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[g_PassConstants.directionalLightBufferIndex][i];
			result += Default_Lit(albedo, F0, light.color, N, V, light.direction, roughness, metalness) * exposure;
		}
	}
	
	
	// directional lights shadowed
	{
		for (uint i = 0; i < g_PassConstants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLightsShadowed[g_PassConstants.directionalLightShadowedBufferIndex][i];
			
			result += Default_Lit(albedo, F0, light.color, N, V, light.direction, roughness, metalness)
				* sampleCascadedShadowMaps(input.worldSpacePosition, N, light) * exposure;
		}
	}
	
	// ambient light
	{
		result += 0.5f * albedo * exposure;
	}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, 1.0f);
	output.normalRoughness = float4(encodeOctahedron24(N), roughness);
	output.albedoMetalness = float4(albedo, metalness);
	
	return output;
}