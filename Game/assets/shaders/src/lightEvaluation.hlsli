#ifndef LIGHT_EVALUTION_H
#define LIGHT_EVALUTION_H

#include "shadingModel.hlsli"
#include "lights.hlsli"

#define LIGHTING_TILE_SIZE 8

struct LightingParams
{
	float3 albedo;
	float3 N;
	float3 V;
	float3 position;
	float metalness;
	float roughness;
};

float smoothDistanceAtt(float squaredDistance, float invSqrAttRadius)
{
	float factor = squaredDistance * invSqrAttRadius;
	float smoothFactor = saturate(1.0 - factor * factor);
	return smoothFactor * smoothFactor;
}

float getDistanceAtt(float3 unnormalizedLightVector, float invSqrAttRadius)
{
	float sqrDist = dot(unnormalizedLightVector, unnormalizedLightVector);
	float attenuation = 1.0 / (max(sqrDist, invSqrAttRadius));
	attenuation *= smoothDistanceAtt(sqrDist, invSqrAttRadius);
	
	return attenuation;
}

float getAngleAtt(float3 L, float3 lightDir, float lightAngleScale, float lightAngleOffset)
{
	float cd = dot(-lightDir, L);
	float attenuation = saturate(cd * lightAngleScale + lightAngleOffset);
	attenuation *= attenuation;
	
	return attenuation;
}

float3 evaluatePunctualLight(const LightingParams params, const PunctualLight light)
{
	const float3 unnormalizedLightVector = light.position - params.position;
	const float3 L = normalize(unnormalizedLightVector);
	float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
	
	if (light.angleScale != -1.0f) // -1.0f is a special value that marks this light as a point light
	{
		att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
	}
	
	const float3 radiance = light.color * att;
	
	const float3 F0 = lerp(0.04f, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, radiance, params.N, params.V, L, params.roughness, params.metalness);
}

float3 evaluateDirectionalLight(const LightingParams params, const DirectionalLight directionalLight)
{
	const float3 F0 = lerp(0.04f, params.albedo, params.metalness);
	return Default_Lit(params.albedo, F0, directionalLight.color, params.N, params.V, directionalLight.direction, params.roughness, params.metalness);
}

uint getTileAddress(uint2 pixelCoord, uint width, uint wordCount)
{
	uint2 tile = pixelCoord / LIGHTING_TILE_SIZE;
	uint address = tile.x + tile.y * ((width + LIGHTING_TILE_SIZE - 1) / LIGHTING_TILE_SIZE);
	return address * wordCount;
}

uint2 getTile(uint2 pixelCoord)
{
	return pixelCoord / LIGHTING_TILE_SIZE;
}

void getLightingMinMaxIndices(ByteAddressBuffer zBinBuffer, uint itemCount, float linearDepth, out uint minIdx, out uint maxIdx, out uint wordMin, out uint wordMax, out uint wordCount)
{
	wordMin = 0;
	wordCount = (itemCount + 31) / 32;
	wordMax = wordCount - 1;
	
	const uint zBinAddress = min(uint(linearDepth), 8191u);
	const uint zBinData = zBinBuffer.Load(zBinAddress * 4);
	const uint minIndex = zBinData >> 16;
	const uint maxIndex = zBinData & uint(0xFFFF);
	// mergedMin scalar from this point
	const uint mergedMin = WaveReadLaneFirst(WaveActiveMin(minIndex)); 
	// mergedMax scalar from this point
	const uint mergedMax = WaveReadLaneFirst(WaveActiveMax(maxIndex)); 
	wordMin = max(mergedMin / 32, wordMin);
	wordMax = min(mergedMax / 32, wordMax);
	
	minIdx = minIndex;
	maxIdx = maxIndex;
}

void getLightingMinMaxIndicesRange(ByteAddressBuffer zBinBuffer, uint itemCount, float linearDepth0, float linearDepth1, out uint minIdx, out uint maxIdx, out uint wordMin, out uint wordMax, out uint wordCount)
{
	float firstDepth = min(linearDepth0, linearDepth1);
	float secondDepth = max(linearDepth0, linearDepth1);
	wordMin = 0;
	wordCount = (itemCount + 31) / 32;
	wordMax = wordCount - 1;
	
	const uint minIndex = zBinBuffer.Load(min(uint(linearDepth0), 8191u) * 4) >> 16;
	const uint maxIndex = zBinBuffer.Load(min(uint(linearDepth1), 8191u) * 4) & uint(0xFFFF);
	// mergedMin scalar from this point
	const uint mergedMin = WaveReadLaneFirst(WaveActiveMin(minIndex)); 
	// mergedMax scalar from this point
	const uint mergedMax = WaveReadLaneFirst(WaveActiveMax(maxIndex)); 
	wordMin = max(mergedMin / 32, wordMin);
	wordMax = min(mergedMax / 32, wordMax);
	
	minIdx = minIndex;
	maxIdx = maxIndex;
}

uint getLightingBitMask(Texture2DArray<uint4> bitMaskTexture, uint2 tile, uint wordIndex, uint minIndex, uint maxIndex)
{
	uint mask = bitMaskTexture.Load(uint4(tile, wordIndex, 0)).x;
	
	// mask by zbin mask
	const int localBaseIndex = int(wordIndex * 32);
	const uint localMin = clamp(int(minIndex) - localBaseIndex, 0, 31);
	const uint localMax = clamp(int(maxIndex) - localBaseIndex, 0, 31);
	const uint maskWidth = localMax - localMin + 1;
	const uint zBinMask = (maskWidth == 32) ? 0xFFFFFFFFu : (((1u << maskWidth) - 1u) << localMin);
	mask &= zBinMask;
	
	// compact word bitmask over all threads in subgroup
	uint mergedMask = WaveReadLaneFirst(WaveActiveBitOr(mask));
	
	return mergedMask;
}

#endif // LIGHT_EVALUTION_H