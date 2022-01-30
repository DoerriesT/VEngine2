#include "bindings.hlsli"
#include "packing.hlsli"
#include "srgb.hlsli"
#include "material.hlsli"
#include "lightEvaluation.hlsli"
#include "irradianceVolume.hlsli"

#ifndef ALPHA_TESTED
#define ALPHA_TESTED 0
#endif

struct PSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	float3 worldSpacePosition : WORLD_SPACE_POS;
	bool frontFace : SV_IsFrontFace;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 distance : SV_Target1;
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
	float4 viewMatrixDepthRow;
	float3 cameraPosition;
	uint transformBufferIndex;
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
	uint irradianceVolumeCount;
	uint irradianceVolumeBufferIndex;
};

struct DrawConstants
{
	uint transformIndex;
	uint materialIndex;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<Material> g_Materials[65536] : REGISTER_SRV(4, 2, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLights[65536] : REGISTER_SRV(4, 3, 1);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 4, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 5, 1);
RWByteAddressBuffer g_RWByteAddressBuffers[65536] : REGISTER_UAV(5, 6, 1);
StructuredBuffer<PunctualLight> g_PunctualLights[65536] : REGISTER_SRV(4, 7, 1);
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed[65536] : REGISTER_SRV(4, 8, 1);
Texture2DArray<uint4> g_ArrayTexturesUI[65536] : REGISTER_SRV(0, 9, 1);
StructuredBuffer<IrradianceVolume> g_IrradianceVolumes[65536] : REGISTER_SRV(4, 10, 1);

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

#if !ALPHA_TESTED
[earlydepthstencil]
#endif
PSOutput main(PSInput input)
{
	LightingParams lightingParams = (LightingParams)0;
	
	Material material = g_Materials[g_PassConstants.materialBufferIndex][g_DrawConstants.materialIndex];
	
	// albedo
	{
		lightingParams.albedo = unpackUnorm4x8(material.albedo).rgb;
		if (material.albedoTextureHandle != 0)
		{
			float4 tap = g_Textures[material.albedoTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord);
			
#if ALPHA_TESTED
			float alpha = tap.a;
			if (alpha < 0.5f)
			{
				discard;
			}
#endif
			
			lightingParams.albedo = tap.rgb;
		}
		lightingParams.albedo = accurateSRGBToLinear(lightingParams.albedo);
	}
	
	// normal (no need for normal mapping when rendering low resolution cubemaps)
	lightingParams.N = normalize(input.normal);
	lightingParams.N = input.frontFace ? lightingParams.N : -lightingParams.N;
	
	// metalness (setting this to 0.0f lets us avoid having to use reflection probes to not get black color where metallic objects are)
	lightingParams.metalness = 0.0f;
	
	// roughness
	{
		lightingParams.roughness = material.roughness;
		if (material.roughnessTextureHandle != 0)
		{
			lightingParams.roughness = g_Textures[material.roughnessTextureHandle].Sample(g_AnisoRepeatSampler, input.texCoord).y;
			lightingParams.roughness = approximateSRGBToLinear(lightingParams.roughness);
		}
	}
	
	lightingParams.V = normalize(g_PassConstants.cameraPosition - input.worldSpacePosition);
	lightingParams.position = input.worldSpacePosition;
	
	float3 result = 0.0f;
	
	result += material.emissive;
	
	// directional lights
	{
		for (uint i = 0; i < g_PassConstants.directionalLightCount; ++i)
		{
			result += evaluateDirectionalLight(lightingParams, g_DirectionalLights[g_PassConstants.directionalLightBufferIndex][i]);
		}
	}
	
	
	// directional lights shadowed
	{
		for (uint i = 0; i < g_PassConstants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[g_PassConstants.directionalLightShadowedBufferIndex][i];
			result += evaluateDirectionalLight(lightingParams, light)
				* sampleCascadedShadowMaps(input.worldSpacePosition, lightingParams.N, light);
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
				result += evaluatePunctualLight(lightingParams, punctualLights[index]);
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
				
				float3 shadowPosWS = lightingParams.position + lightingParams.N * 0.02f;
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
				
				result += evaluatePunctualLight(lightingParams, lightShadowed.light) * shadow;
			}
		}
	}
	
	// indirect diffuse
	{
		float4 sum = 0.0f;
		for (uint i = 0; i < g_PassConstants.irradianceVolumeCount; ++i)
		{
			IrradianceVolume volume = g_IrradianceVolumes[g_PassConstants.irradianceVolumeBufferIndex][i];
			Texture2D<float4> diffuseTex = g_Textures[volume.diffuseTextureIndex];
			Texture2D<float4> visibilityTex = g_Textures[volume.visibilityTextureIndex];
			
			sum += sampleIrradianceVolume(diffuseTex, visibilityTex, g_LinearClampSampler, volume, lightingParams.position, lightingParams.N, lightingParams.V);
		}
		
		if (sum.a > 1e-5f)
		{
			result += (sum.rgb / sum.a) * lightingParams.albedo;
		}
	}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, 1.0f);
	output.distance = distance(lightingParams.position, g_PassConstants.cameraPosition);
	
	return output;
}