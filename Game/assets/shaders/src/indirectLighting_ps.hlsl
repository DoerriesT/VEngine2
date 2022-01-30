#include "bindings.hlsli"
#include "brdf.hlsli"
#include "srgb.hlsli"
#include "packing.hlsli"
#include "irradianceVolume.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD;
};

struct Constants
{
	float4x4 invViewProjectionMatrix;
	float3 volumetricFogTexelSize;
	float volumetricFogNear;
	float2 depthUnprojectParams;
	float2 texelSize;
	float3 cameraPosition;
	float volumetricFogFar;
	uint exposureBufferIndex;
	uint albedoTextureIndex;
	uint normalTextureIndex;
	uint gtaoTextureIndex;
	uint depthBufferIndex;
	uint volumetricFogTextureIndex;
	uint reflectionProbeTextureIndex;
	uint brdfLUTIndex;
	uint blueNoiseTextureIndex;
	uint reflectionProbeDataBufferIndex;
	uint frame;
	uint reflectionProbeCount;
	uint irradianceVolumeCount;
	uint irradianceVolumeBufferIndex;
};

struct ReflectionProbe
{
	float4 worldToLocal0;
	float4 worldToLocal1;
	float4 worldToLocal2;
	float3 capturePosition;
	float arraySlot;
	float boxInvFadeDist0;
	float boxInvFadeDist1;
	float boxInvFadeDist2;
	float boxInvFadeDist3;
	float boxInvFadeDist4;
	float boxInvFadeDist5;
	float pad0;
	float pad1;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
Texture3D<float4> g_Textures3D[65536] : REGISTER_SRV(0, 1, 1);
TextureCubeArray<float4> g_CubeArrayTextures[65536] : REGISTER_SRV(0, 2, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 3, 1);
Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 4, 1);
StructuredBuffer<ReflectionProbe> g_ReflectionProbes[65536] : REGISTER_SRV(4, 5, 1);
StructuredBuffer<IrradianceVolume> g_IrradianceVolumes[65536] : REGISTER_SRV(4, 6, 1);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

float3 inverseSimpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	return color / max(1.0f - luma, 1e-6f);
}

float3 parallaxCorrectReflectionDir(const ReflectionProbe probeData, float3 position, float3 reflectedDir, out float alpha)
{
	// Intersection with OBB convert to unit box space
	// Transform in local unit parallax cube space (scaled and rotated)
	const float3 positionLS = float3(dot(probeData.worldToLocal0, float4(position, 1.0f)),
									dot(probeData.worldToLocal1, float4(position, 1.0f)),
									dot(probeData.worldToLocal2, float4(position, 1.0f)));

	if (any(abs(positionLS) > 1.0f))
	{
		alpha = 0.0f;
		return reflectedDir;
	}

	const float3 rayLS = float3(dot(probeData.worldToLocal0.xyz, reflectedDir), 
							dot(probeData.worldToLocal1.xyz, reflectedDir), 
							dot(probeData.worldToLocal2.xyz, reflectedDir));
	const float3 invRayLS = rcp(rayLS);
	
	float3 firstPlaneIntersect  = (1.0f - positionLS) * invRayLS;
	float3 secondPlaneIntersect = (-1.0f - positionLS) * invRayLS;
	float3 furthestPlane = max(firstPlaneIntersect, secondPlaneIntersect);
	float dist = min(furthestPlane.x, min(furthestPlane.y, furthestPlane.z));
	
	// Use distance in WS directly to recover intersection
	float3 intersectPositionWS = position + reflectedDir * dist;
	
	alpha = 1.0f;
	
	// (1.0 + positionLS.x) * probeData.boxInvFadeDist1
	// = positionLS.x * probeData.boxInvFadeDist1 + probeData.boxInvFadeDist1
	
	alpha *= smoothstep(0.0f, 1.0f, saturate(positionLS.x > 0.0f ? 
									mad(-positionLS.x, probeData.boxInvFadeDist0, probeData.boxInvFadeDist0) : 
									mad(positionLS.x, probeData.boxInvFadeDist1, probeData.boxInvFadeDist1)));
	alpha *= smoothstep(0.0f, 1.0f, saturate(positionLS.y > 0.0f ? 
									mad(-positionLS.y, probeData.boxInvFadeDist2, probeData.boxInvFadeDist2) : 
									mad(positionLS.y, probeData.boxInvFadeDist3, probeData.boxInvFadeDist3)));
	alpha *= smoothstep(0.0f, 1.0f, saturate(positionLS.z > 0.0f ? 
									mad(-positionLS.z, probeData.boxInvFadeDist4, probeData.boxInvFadeDist4) : 
									mad(positionLS.z, probeData.boxInvFadeDist5, probeData.boxInvFadeDist5)));
	
	return intersectPositionWS - probeData.capturePosition;
}

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	const float depth = g_Textures[g_Constants.depthBufferIndex].Load(int3(input.position.xy, 0)).x;
	
	const float4 worldSpacePos4 = mul(g_Constants.invViewProjectionMatrix, float4(input.uv * float2(2.0f, -2.0f) - float2(1.0f, -1.0f), depth, 1.0f));
	const float3 worldSpacePos = worldSpacePos4.xyz / worldSpacePos4.w;
	
	const float exposure = asfloat(g_ByteAddressBuffers[g_Constants.exposureBufferIndex].Load(0));
	
	const float4 albedoMetalness = g_Textures[g_Constants.albedoTextureIndex].Load(uint3(input.position.xy, 0));
	const float3 albedo = albedoMetalness.rgb;
	const float metalness = approximateSRGBToLinear(albedoMetalness.w);
	const float4 normalRoughness = g_Textures[g_Constants.normalTextureIndex].Load(uint3(input.position.xy, 0));
	const float3 N = decodeOctahedron24(normalRoughness.xyz);
	const float roughness = approximateSRGBToLinear(normalRoughness.w);
	const float3 V = normalize(g_Constants.cameraPosition - worldSpacePos);
	const float3 F0 = lerp(0.04f, albedo, metalness);
	
	const float gtao = g_Textures[g_Constants.gtaoTextureIndex].Load(uint3(input.position.xy, 0)).x;
	
	float3 result = 0.0f;
	
	// indirect diffuse
	{
		float4 sum = 0.0f;
		for (uint i = 0; i < g_Constants.irradianceVolumeCount; ++i)
		{
			IrradianceVolume volume = g_IrradianceVolumes[g_Constants.irradianceVolumeBufferIndex][i];
			Texture2D<float4> diffuseTex = g_Textures[volume.diffuseTextureIndex];
			Texture2D<float4> visibilityTex = g_Textures[volume.visibilityTextureIndex];
			
			sum += sampleIrradianceVolume(diffuseTex, visibilityTex, g_LinearClampSampler, volume, worldSpacePos, N, V);
		}
		
		if (sum.a > 1e-5f)
		{
			result += (sum.rgb / sum.a) * albedo * (1.0f - metalness) * gtao * exposure;
		}
	}
	
	// reflection probe
	if (g_Constants.reflectionProbeCount > 0)
	{
		const float maxMip = 3.0f;
		const float brdfLUTRes = 128.0f;
		const float brdfLUTMinUV = 0.5f / brdfLUTRes;
		const float brdfLUTMaxUV = (brdfLUTRes - 0.5f) / brdfLUTRes;
		
		float3 probeSum = 0.0f;
		float weightSum = 0.0f;
		
		for (uint i = 0; i < g_Constants.reflectionProbeCount; ++i)
		{
			ReflectionProbe probeData = g_ReflectionProbes[g_Constants.reflectionProbeDataBufferIndex][i];
			
			float probeAlpha = 0.0f;
			float3 sampleDir = parallaxCorrectReflectionDir(probeData, worldSpacePos, reflect(-V, N), probeAlpha);
			
			if (probeAlpha == 0.0f)
			{
				continue;
			}
			
			// sample probe
			float mip = sqrt(roughness) * maxMip;
			float3 probe = g_CubeArrayTextures[g_Constants.reflectionProbeTextureIndex].SampleLevel(g_LinearClampSampler, float4(sampleDir, probeData.arraySlot), mip).rgb;
			
			probeSum += probe * probeAlpha;
			weightSum += probeAlpha;
		}
		
		if (weightSum > 1e-5f)
		{
			// sample brdf LUT
			float2 brdfLUTUV = clamp(float2(saturate(dot(N, V)), roughness), brdfLUTMinUV, brdfLUTMaxUV);
			float2 brdfLut = g_Textures[g_Constants.brdfLUTIndex].SampleLevel(g_LinearClampSampler, brdfLUTUV, 0.0f).xy;
			
			probeSum /= weightSum;
			
			probeSum *= F0 * brdfLut.x + brdfLut.y;
			result += probeSum * exposure;
		}
		
	}
	
	// volumetric fog
	float fogAttenuation = 1.0f;
	{
		float4 noise = g_ArrayTextures[g_Constants.blueNoiseTextureIndex].Load(int4(int2(input.position.xy) & 63, g_Constants.frame & 63, 0));
		noise = (noise * 2.0f - 1.0f) * 1.5f;
		
		const float linearDepth = rcp(g_Constants.depthUnprojectParams.x * depth + g_Constants.depthUnprojectParams.y);
		float d = (log2(max(0, linearDepth * rcp(g_Constants.volumetricFogNear))) * rcp(log2(g_Constants.volumetricFogFar / g_Constants.volumetricFogNear)));
	
		d = max(0.0f, d - g_Constants.volumetricFogTexelSize.z * 1.5f);
		
		float4 fog = 0.0f;
		for (int i = 0; i < 4; ++i)
		{
			float3 tc = float3(input.uv, d) + noise.xyz * float3(1.0f, 1.0f, 0.5f) * g_Constants.volumetricFogTexelSize;
			fog += g_Textures3D[g_Constants.volumetricFogTextureIndex].SampleLevel(g_LinearClampSampler, tc, 0.0f) / 4.0f;
			noise = noise.yzwx;
		}
		fog.rgb = inverseSimpleTonemap(fog.rgb);
		
		result = result * fog.a + fog.rgb;
		fogAttenuation = fog.a;
	}
	
	return float4(result, fogAttenuation);
}