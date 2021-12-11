#include "bindings.hlsli"
#include "lightEvaluation.hlsli"

struct Constants
{
	float4x4 prevViewMatrix;
	float4x4 prevProjMatrix;
	float4 volumeResResultRes;
	float4 viewMatrixDepthRow;
	float3 frustumCornerTL;
	float volumeDepth;
	float3 frustumCornerTR;
	float volumeNear;
	float3 frustumCornerBL;
	float volumeFar;
	float3 frustumCornerBR;
	float jitterX;
	float3 cameraPos;
	float jitterY;
	float2 volumeTexelSize;
	float jitterZ;
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
	uint globalMediaCount;
	uint globalMediaBufferIndex;
	uint localMediaCount;
	uint localMediaBufferIndex;
	uint localMediaTileTextureIndex;
	uint localMediaDepthBinsBufferIndex;
	uint exposureBufferIndex;
	uint scatterResultTextureIndex;
	uint filterInputTextureIndex;
	uint filterHistoryTextureIndex;
	uint filterResultTextureIndex;
	uint integrateInputTextureIndex;
	uint integrateResultTextureIndex;
	float filterAlpha;
	uint checkerBoardCondition;
	uint ignoreHistory;
};

struct GlobalParticipatingMedium
{
	float3 emissive;
	float extinction;
	float3 scattering;
	float phase;
	uint heightFogEnabled;
	float heightFogStart;
	float heightFogFalloff;
	float maxHeight;
	float3 textureBias;
	float textureScale;
	uint densityTexture;
	float pad0;
	float pad1;
	float pad2;
};

struct LocalParticipatingMedium
{
	float4 worldToLocal0;
	float4 worldToLocal1;
	float4 worldToLocal2;
	float3 position;
	float phase;
	float3 emissive;
	float extinction;
	float3 scattering;
	uint densityTexture;
	float3 textureScale;
	float heightFogStart;
	float3 textureBias;
	float heightFogFalloff;
	uint spherical;
	float pad0;
	float pad1;
	float pad2;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
Texture3D<float4> g_Textures3D[65536] : REGISTER_SRV(0, 1, 1);
Texture2DArray<uint4> g_ArrayTexturesUI[65536] : REGISTER_SRV(0, 2, 1);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 3, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLights[65536] : REGISTER_SRV(4, 4, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLightsShadowed[65536] : REGISTER_SRV(4, 5, 1);
StructuredBuffer<PunctualLight> g_PunctualLights[65536] : REGISTER_SRV(4, 6, 1);
StructuredBuffer<PunctualLightShadowed> g_PunctualLightsShadowed[65536] : REGISTER_SRV(4, 7, 1);
StructuredBuffer<GlobalParticipatingMedium> g_GlobalMedia[65536] : REGISTER_SRV(4, 8, 1);
StructuredBuffer<LocalParticipatingMedium> g_LocalMedia[65536] : REGISTER_SRV(4, 9, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 10, 1);
RWTexture3D<float4> g_RWTextures3D[65536] : REGISTER_UAV(1, 11, 1);


SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);
SamplerComparisonState g_ShadowSampler : REGISTER_SAMPLER(1, 0, 2);

float volumetricFogGetDensity(GlobalParticipatingMedium medium, float3 position)
{
	float density = position.y > medium.maxHeight ? 0.0f : 1.0f;
	if (medium.heightFogEnabled != 0 && position.y > medium.heightFogStart)
	{
		float h = position.y - medium.heightFogStart;
		density *= exp(-h * medium.heightFogFalloff);
	}
	if (medium.densityTexture != 0)
	{
		float3 uv = frac(position * medium.textureScale + medium.textureBias);
		float noise = g_Textures3D[medium.densityTexture].SampleLevel(g_LinearClampSampler, uv, 0.0).x;
		//float noise = saturate(perlinNoise() * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= noise;
	}
	return density;
}

float volumetricFogGetDensity(LocalParticipatingMedium medium, float3 position)
{
	position = position * 0.5f + 0.5f;
	float density = 1.0f;
	if (medium.heightFogStart < 1.0f && position.y > medium.heightFogStart)
	{
		float h = position.y - medium.heightFogStart;
		density *= exp(-h * medium.heightFogFalloff);
	}
	if (medium.densityTexture != 0)
	{
		float3 uv = frac(position * medium.textureScale + medium.textureBias);
		float noise = g_Textures3D[medium.densityTexture].SampleLevel(g_LinearClampSampler, uv, 0.0).x;
		//float noise = saturate(perlinNoise() * 0.5 + 0.5) * saturate(medium.noiseIntensity);
		density *= noise;
	}
	return density;
}

float3 calcWorldSpacePos(float3 texelCoord)
{
	float2 uv = texelCoord.xy * g_Constants.volumeTexelSize.xy;
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * rcp(g_Constants.volumeDepth);
	float z = g_Constants.volumeNear * exp2(d * (log2(g_Constants.volumeFar / g_Constants.volumeNear)));
	pos *= z / g_Constants.volumeFar;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

float getDirectionalLightShadow(const DirectionalLight light, float3 worldSpacePos)
{
	float4 shadowCoord = 0.0f;
	uint cascadeIndex = 0;
	bool foundCascade = false;
	for (; cascadeIndex < light.cascadeCount; ++cascadeIndex)
	{
		shadowCoord = mul(light.shadowMatrices[cascadeIndex], float4(worldSpacePos, 1.0f));
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

float henyeyGreenstein(float3 V, float3 L, float g)
{
	float num = -g * g + 1.0f;
	float cosTheta = dot(-L, V);
	float denom = 4.0 * PI * pow((g * g + 1.0f) - 2.0f * g * cosTheta, 1.5f);
	return num / denom;
}

void vbuffer(uint2 coord, float3 worldSpacePos, float linearDepth, out float3 scattering, out float extinction, out float3 emissive, out float phase)
{
	scattering = 0.0f;
	extinction = 0.0f;
	emissive = 0.0f;
	phase = 0.0f;
	uint accumulatedMediaCount = 0;
	
	// iterate over all global participating media
	{
		for (int i = 0; i < g_Constants.globalMediaCount; ++i)
		{
			GlobalParticipatingMedium medium = g_GlobalMedia[g_Constants.globalMediaBufferIndex][i];
			const float density = volumetricFogGetDensity(medium, worldSpacePos);
			scattering += medium.scattering * density;
			extinction += medium.extinction * density;
			emissive += medium.emissive * density;
			phase += medium.phase;
			++accumulatedMediaCount;
		}
	}

	// iterate over all local participating media
	uint localMediaCount = g_Constants.localMediaCount;
	if (localMediaCount > 0)
	{
		uint wordMin, wordMax, minIndex, maxIndex, wordCount;
		getLightingMinMaxIndices(g_ByteAddressBuffers[g_Constants.localMediaDepthBinsBufferIndex], localMediaCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
		const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
		Texture2DArray<uint4> tileTex = g_ArrayTexturesUI[g_Constants.localMediaTileTextureIndex];
		StructuredBuffer<LocalParticipatingMedium> localMedia = g_LocalMedia[g_Constants.localMediaBufferIndex];
	
		for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
		{
			uint mask = getLightingBitMask(tileTex, tile, wordIndex, minIndex, maxIndex);
			
			while (mask != 0)
			{
				const uint bitIndex = firstbitlow(mask);
				const uint index = 32 * wordIndex + bitIndex;
				mask ^= (1u << bitIndex);
				
				LocalParticipatingMedium medium = localMedia[index];
				
				const float3 localPos = float3(dot(medium.worldToLocal0, float4(worldSpacePos, 1.0f)), 
								dot(medium.worldToLocal1, float4(worldSpacePos, 1.0f)), 
								dot(medium.worldToLocal2, float4(worldSpacePos, 1.0f)));
								
				if (all(abs(localPos) <= 1.0f) && (medium.spherical == 0 || dot(localPos, localPos) <= 1.0f))
				{
					float density = volumetricFogGetDensity(medium, localPos);
					scattering += medium.scattering * density;
					extinction += medium.extinction * density;
					emissive += medium.emissive * density;
					phase += medium.phase;
					++accumulatedMediaCount;
				}
			}
		}
	}
	
	phase = accumulatedMediaCount > 0 ? phase * rcp((float)accumulatedMediaCount) : 0.0f;
}

float4 inscattering(uint2 coord, float3 V, float3 worldSpacePos, float linearDepth, float3 scattering, float extinction, float3 emissive, float phase)
{
	// integrate inscattered lighting
	float3 lighting = emissive;
	{
		// ambient
		{
			float3 ambientLight = (1.0f / PI);
			lighting += ambientLight / (4.0f * PI);
		}
		
		// directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLights[g_Constants.directionalLightBufferIndex][i];
				lighting += directionalLight.color * henyeyGreenstein(V, directionalLight.direction, phase);
			}
		}
		
		// shadowed directional lights
		{
			for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
			{
				DirectionalLight directionalLight = g_DirectionalLightsShadowed[g_Constants.directionalLightShadowedBufferIndex][i];
				float shadow = getDirectionalLightShadow(directionalLight, worldSpacePos);
				lighting += directionalLight.color * henyeyGreenstein(V, directionalLight.direction, phase) * shadow;
			}
		}
		
		// punctual lights
		uint punctualLightCount = g_Constants.punctualLightCount;
		if (punctualLightCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_ByteAddressBuffers[g_Constants.punctualLightDepthBinsBufferIndex], punctualLightCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
			Texture2DArray<uint4> tileTex = g_ArrayTexturesUI[g_Constants.punctualLightTileTextureIndex];
			StructuredBuffer<PunctualLight> punctualLights = g_PunctualLights[g_Constants.punctualLightBufferIndex];
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(tileTex, tile, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1u << bitIndex);
					
					PunctualLight light = punctualLights[index];
					
					const float3 unnormalizedLightVector = light.position - worldSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
					
					if (isSpotLight(light))
					{
						att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
					}
					
					const float3 radiance = light.color * att;
					
					lighting += radiance * henyeyGreenstein(V, L, phase);
				}
			}
		}
		
		// punctual lights shadowed
		uint punctualLightShadowedCount = g_Constants.punctualLightShadowedCount;
		if (punctualLightShadowedCount > 0)
		{
			uint wordMin, wordMax, minIndex, maxIndex, wordCount;
			getLightingMinMaxIndices(g_ByteAddressBuffers[g_Constants.punctualLightShadowedDepthBinsBufferIndex], punctualLightShadowedCount, linearDepth, minIndex, maxIndex, wordMin, wordMax, wordCount);
			const uint2 tile = getTile(coord / g_Constants.volumeResResultRes.xy * g_Constants.volumeResResultRes.zw);
	
			Texture2DArray<uint4> tileTex = g_ArrayTexturesUI[g_Constants.punctualLightShadowedTileTextureIndex];
			StructuredBuffer<PunctualLightShadowed> punctualLights = g_PunctualLightsShadowed[g_Constants.punctualLightShadowedBufferIndex];
	
			for (uint wordIndex = wordMin; wordIndex <= wordMax; ++wordIndex)
			{
				uint mask = getLightingBitMask(tileTex, tile, wordIndex, minIndex, maxIndex);
				
				while (mask != 0)
				{
					const uint bitIndex = firstbitlow(mask);
					const uint index = 32 * wordIndex + bitIndex;
					mask ^= (1u << bitIndex);
					
					const PunctualLightShadowed lightShadowed = punctualLights[index];
					const PunctualLight light = lightShadowed.light;
					
					uint shadowMapIndex = 0;
					float normalizedDistance = 0.0f;
					float3 shadowPos = calculatePunctualLightShadowPos(lightShadowed, worldSpacePos, shadowMapIndex, normalizedDistance);
					float shadow = g_Textures[NonUniformResourceIndex(shadowMapIndex)].SampleCmpLevelZero(g_ShadowSampler, shadowPos.xy, shadowPos.z).x;
					
					const float3 unnormalizedLightVector = light.position - worldSpacePos;
					const float3 L = normalize(unnormalizedLightVector);
					float att = getDistanceAtt(unnormalizedLightVector, light.invSqrAttRadius);
					
					if (isSpotLight(lightShadowed.light))
					{
						att *= getAngleAtt(L, light.direction, light.angleScale, light.angleOffset);
					}
					
					const float3 radiance = light.color * att * shadow;
					
					lighting += radiance * henyeyGreenstein(V, L, phase);
				}
			}
		}
	}
	
	float4 result = float4(lighting * scattering, extinction);
	
	// apply pre-exposure
	result.rgb *= asfloat(g_ByteAddressBuffers[g_Constants.exposureBufferIndex].Load(0));
	
	return result;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float3 texelCoord = threadID;

	// checkerboard
	texelCoord.z *= 2.0f;
	texelCoord.z += (((threadID.x + threadID.y) & 1) == g_Constants.checkerBoardCondition) ? 1.0f : 0.0f;

	texelCoord += float3(g_Constants.jitterX, g_Constants.jitterY, frac(g_Constants.jitterZ));
	
	const float3 worldSpacePos = calcWorldSpacePos(texelCoord);
	const float linearDepth = -dot(g_Constants.viewMatrixDepthRow, float4(worldSpacePos, 1.0f));

	float3 scattering = 0.0f;
	float extinction = 0.0f;
	float3 emissive = 0.0f;
	float phase = 0.0f;
	
	vbuffer(threadID.xy, worldSpacePos, linearDepth, scattering, extinction, emissive, phase);

	const float3 V = normalize(g_Constants.cameraPos - worldSpacePos);
	g_RWTextures3D[g_Constants.scatterResultTextureIndex][threadID] = inscattering(threadID.xy, V, worldSpacePos, linearDepth, scattering, extinction, emissive, phase);
}