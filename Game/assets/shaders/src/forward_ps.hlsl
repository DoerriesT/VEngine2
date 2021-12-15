#include "bindings.hlsli"
#include "packing.hlsli"
#include "srgb.hlsli"
#include "material.hlsli"
#include "lightEvaluation.hlsli"
#include "picking.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	float3 worldSpacePosition : WORLD_SPACE_POS;
	float4 curScreenPos : CUR_SCREEN_POS;
	float4 prevScreenPos : PREV_SCREEN_POS;
	bool frontFace : SV_IsFrontFace;
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
	float4x4 jitteredViewProjectionMatrix;
	float4x4 viewProjectionMatrix;
	float4x4 prevViewProjectionMatrix;
	float4 viewMatrixDepthRow;
	float3 cameraPosition;
	uint transformBufferIndex;
	uint prevTransformBufferIndex;
	uint skinningMatricesBufferIndex;
	uint prevSkinningMatricesBufferIndex;
	uint materialBufferIndex;
	uint directionalLightCount;
	uint directionalLightBufferIndex;
	uint directionalLightShadowedCount;
	uint directionalLightShadowedBufferIndex;
	uint punctualLightCount;
	uint punctualLightBufferIndex;
	uint punctualLightTileTextureIndex;
	uint punctualLightDepthBinsBufferIndex;
	uint punctualLightShadowedCount;
	uint punctualLightShadowedBufferIndex;
	uint punctualLightShadowedTileTextureIndex;
	uint punctualLightShadowedDepthBinsBufferIndex;
	uint exposureBufferIndex;
	uint pickingBufferIndex;
	uint pickingPosX;
	uint pickingPosY;
	float lodBias;
};

struct DrawConstants
{
	uint transformIndex;
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
StructuredBuffer<PunctualLight> g_PunctualLights[65536] : REGISTER_SRV(4, 8, 1);
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed[65536] : REGISTER_SRV(4, 9, 1);
Texture2DArray<uint4> g_ArrayTexturesUI[65536] : REGISTER_SRV(0, 10, 1);

SamplerState g_AnisoRepeatSampler : REGISTER_SAMPLER(0, 0, 2);
SamplerState g_LinearClampSampler : REGISTER_SAMPLER(1, 0, 2);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(2, 0, 2);

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

float interleavedGradientNoise(float2 v)
{
	float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
	return frac(magic.z * dot(v, magic.xy));
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
	
	LightingParams lightingParams = (LightingParams)0;
	
	Material material = g_Materials[g_PassConstants.materialBufferIndex][g_DrawConstants.materialIndex];
	
	// albedo
	{
		lightingParams.albedo = unpackUnorm4x8(material.albedo).rgb;
		if (material.albedoTextureHandle != 0)
		{
			lightingParams.albedo = g_Textures[material.albedoTextureHandle].SampleBias(g_AnisoRepeatSampler, input.texCoord, g_PassConstants.lodBias).rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(lightingParams.albedo);
	}
	
	// normal
	lightingParams.N = normalize(input.normal);
	lightingParams.N = input.frontFace ? lightingParams.N : -lightingParams.N;
	float3 vertexNormal = lightingParams.N;
	{
		if (material.normalTextureHandle != 0)
		{
			float3 tangentSpaceNormal;
			tangentSpaceNormal.xy = g_Textures[material.normalTextureHandle].SampleBias(g_AnisoRepeatSampler, input.texCoord, g_PassConstants.lodBias).xy * 2.0 - 1.0;
			tangentSpaceNormal.z = sqrt(1.0 - tangentSpaceNormal.x * tangentSpaceNormal.x + tangentSpaceNormal.y * tangentSpaceNormal.y);

			float3 bitangent = cross(input.normal, input.tangent.xyz) * input.tangent.w;
			lightingParams.N = tangentSpaceNormal.x * input.tangent.xyz + tangentSpaceNormal.y * bitangent + tangentSpaceNormal.z * input.normal;
			
			lightingParams.N = normalize(lightingParams.N);
			lightingParams.N = any(isnan(lightingParams.N)) ? normalize(input.normal) : lightingParams.N;
		}
	}
	
	// metalness
	{
		lightingParams.metalness = material.metalness;
		if (material.metalnessTextureHandle != 0)
		{
			lightingParams.metalness = g_Textures[material.metalnessTextureHandle].SampleBias(g_AnisoRepeatSampler, input.texCoord, g_PassConstants.lodBias).z;
		}
	}
	
	// roughness
	{
		lightingParams.roughness = material.roughness;
		if (material.roughnessTextureHandle != 0)
		{
			lightingParams.roughness = g_Textures[material.roughnessTextureHandle].SampleBias(g_AnisoRepeatSampler, input.texCoord, g_PassConstants.lodBias).y;
		}
	}
	
	lightingParams.V = normalize(g_PassConstants.cameraPosition - input.worldSpacePosition);
	lightingParams.position = input.worldSpacePosition;
	
	const float exposure = asfloat(g_ByteAddressBuffers[g_PassConstants.exposureBufferIndex].Load(0));
	
	float3 result = 0.0f;
	
	// directional lights
	{
		for (uint i = 0; i < g_PassConstants.directionalLightCount; ++i)
		{
			result += evaluateDirectionalLight(lightingParams, g_DirectionalLights[g_PassConstants.directionalLightBufferIndex][i]) * exposure;
		}
	}
	
	
	// directional lights shadowed
	{
		for (uint i = 0; i < g_PassConstants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLightsShadowed[g_PassConstants.directionalLightShadowedBufferIndex][i];
			result += evaluateDirectionalLight(lightingParams, light)
				* sampleCascadedShadowMaps(input.worldSpacePosition, lightingParams.N, light) * exposure;
		}
	}
	
	const float linearDepth = -dot(g_PassConstants.viewMatrixDepthRow, float4(lightingParams.position, 1.0));
	
	// punctual lights
	uint punctualLightCount = g_PassConstants.punctualLightCount;
	if (punctualLightCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_ByteAddressBuffers[g_PassConstants.punctualLightDepthBinsBufferIndex], punctualLightCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint2 tile = getTile(int2(input.position.xy));

		Texture2DArray<uint4> tileTex = g_ArrayTexturesUI[g_PassConstants.punctualLightTileTextureIndex];
		StructuredBuffer<PunctualLight> punctualLights = g_PunctualLights[g_PassConstants.punctualLightBufferIndex];

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(tileTex, tile, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1u << bitIndex);
				result += evaluatePunctualLight(lightingParams, punctualLights[index]) * exposure;
			}
		}
	}
	
	float noise = interleavedGradientNoise(input.position.xy);
	
	// punctual lights shadowed
	uint punctualLightShadowedCount = g_PassConstants.punctualLightShadowedCount;
	if (punctualLightShadowedCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_ByteAddressBuffers[g_PassConstants.punctualLightShadowedDepthBinsBufferIndex], punctualLightShadowedCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint2 tile = getTile(int2(input.position.xy));

		Texture2DArray<uint4> tileTex = g_ArrayTexturesUI[g_PassConstants.punctualLightShadowedTileTextureIndex];
		StructuredBuffer<PunctualLightShadowed> punctualLights = g_PunctualLightsShadowed[g_PassConstants.punctualLightShadowedBufferIndex];

		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(tileTex, tile, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1u << bitIndex);
				
				PunctualLightShadowed lightShadowed = punctualLights[index];
				
				float3 shadowPosWS = lightingParams.position + vertexNormal * 0.02f;
				uint shadowMapIndex = 0;
				float normalizedDistance = 0.0f;
				float3 shadowPos = calculatePunctualLightShadowPos(lightShadowed, shadowPosWS, shadowMapIndex, normalizedDistance);
				
				float shadow = pcfShadowContactHardening(
					lightShadowed, 
					g_Textures[NonUniformResourceIndex(shadowMapIndex)], 
					g_LinearClampSampler, 
					shadowPos.xy, 
					normalizedDistance, 
					noise, 
					8
				);
				
				result += evaluatePunctualLight(lightingParams, lightShadowed.light) * shadow * exposure;
			}
		}
	}
	
	//// ambient light
	//{
	//	result += 0.5f * albedo * exposure;
	//}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, 1.0f);
	output.normalRoughness = float4(encodeOctahedron24(lightingParams.N), lightingParams.roughness);
	output.albedoMetalness = float4(lightingParams.albedo, lightingParams.metalness);
	
	float2 prevUV = (input.prevScreenPos.xy / input.prevScreenPos.w) * float2(0.5f, -0.5f) + 0.5f;
	float2 curUV = (input.curScreenPos.xy / input.curScreenPos.w) * float2(0.5f, -0.5f) + 0.5f;

	output.velocity = curUV - prevUV;
	
	return output;
}