#include "bindings.hlsli"
#include "packing.hlsli"
#include "lightEvaluation.hlsli"
#include "srgb.hlsli"
#include "irradianceVolume.hlsli"

struct Constants
{
	float4x4 probeFaceToWorldSpace[6];
	float4 worldToLocal0;
	float4 worldToLocal1;
	float4 worldToLocal2;
	float3 probePosition;
	float texelSize;
	uint resolution;
	uint probeArraySlot;
	uint directionalLightCount;
	uint directionalLightBufferIndex;
	uint directionalLightShadowedCount;
	uint directionalLightShadowedBufferIndex;
	uint irradianceVolumeCount;
	uint irradianceVolumeBufferIndex;
	uint albedoRoughnessTextureIndex;
	uint normalDepthTextureIndex;
	uint resultTextureIndex;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2DArray<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLights[65536] : REGISTER_SRV(4, 1, 1);
Texture2D<float4> g_Textures2D[65536] : REGISTER_SRV(0, 2, 1);
StructuredBuffer<IrradianceVolume> g_IrradianceVolumes[65536] : REGISTER_SRV(4, 3, 1);
RWTexture2DArray<float4> g_RWTextures[65536] : REGISTER_UAV(1, 4, 1);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

float getApproxShadow(float3 rayOrigin, float3 rayDir)
{
	// Intersection with OBB convert to unit box space
	// Transform in local unit parallax cube space (scaled and rotated)
	const float3 positionLS = float3(dot(g_Constants.worldToLocal0, float4(rayOrigin, 1.0f)),
									dot(g_Constants.worldToLocal1, float4(rayOrigin, 1.0f)),
									dot(g_Constants.worldToLocal2, float4(rayOrigin, 1.0f)));

	const float3 rayLS = float3(dot(g_Constants.worldToLocal0.xyz, rayDir), 
							dot(g_Constants.worldToLocal1.xyz, rayDir), 
							dot(g_Constants.worldToLocal2.xyz, rayDir));
	const float3 invRayLS = rcp(rayLS);
	
	float3 firstPlaneIntersect  = (1.0f - positionLS) * invRayLS;
	float3 secondPlaneIntersect = (-1.0f - positionLS) * invRayLS;
	float3 furthestPlane = max(firstPlaneIntersect, secondPlaneIntersect);
	float dist = min(furthestPlane.x, min(furthestPlane.y, furthestPlane.z));
	
	// Use distance in WS directly to recover intersection
	float3 intersectPositionWS = rayOrigin + rayDir * dist;
	
	float3 sampleDir = intersectPositionWS - g_Constants.probePosition;
	
	int faceIdx = 0;
	float2 uv = sampleCube(sampleDir, faceIdx);
	float arraySlice = g_Constants.probeArraySlot * 6 + faceIdx;
	
	return g_Textures[g_Constants.normalDepthTextureIndex].SampleLevel(g_LinearClampSampler, float3(uv, arraySlice), 0.0f).z == 0.0f;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_Constants.resolution))
	{
		return;
	}
	
	float3 normalDepth = g_Textures[g_Constants.normalDepthTextureIndex].Load(int4(int3(threadID.xy, threadID.z + g_Constants.probeArraySlot * 6), 0)).xyz;
	float depth = normalDepth.z;
	
	float2 uv = (threadID.xy + 0.5f) * g_Constants.texelSize;
	
	float4 worldSpacePos4 = mul(g_Constants.probeFaceToWorldSpace[threadID.z], float4(uv * float2(2.0f, -2.0f) - float2(1.0f, -1.0f), depth, 1.0f));
	float3 worldSpacePos = worldSpacePos4.xyz / worldSpacePos4.w;
	
	float3 V = normalize(g_Constants.probePosition - worldSpacePos);
	
	if (depth == 0.0f)
	{
		float3 skyColor = accurateSRGBToLinear(float3(0.529f, 0.808f, 0.922f));
		float3 groundColor = accurateSRGBToLinear(float3(240.0f, 216.0f, 195.0f) / 255.0f);
		
		float3 sky = lerp(groundColor, skyColor, V.y * 0.5f + 0.5f);
		
		g_RWTextures[g_Constants.resultTextureIndex][threadID] = float4(sky, 1.0f);
		return;
	}
	
	float3 N = decodeOctahedron(normalDepth.xy);
	float4 albedoRoughness = g_Textures[g_Constants.albedoRoughnessTextureIndex].Load(int4(int3(threadID.xy, threadID.z + g_Constants.probeArraySlot * 6), 0));
	float3 albedo = albedoRoughness.rgb;
	float roughness = approximateSRGBToLinear(albedoRoughness.w);
	
	float3 result = 0.0f;
	
	// indirect diffuse
	{
		float4 sum = 0.0f;
		for (uint i = 0; i < g_Constants.irradianceVolumeCount; ++i)
		{
			IrradianceVolume volume = g_IrradianceVolumes[g_Constants.irradianceVolumeBufferIndex][i];
			Texture2D<float4> diffuseTex = g_Textures2D[volume.diffuseTextureIndex];
			Texture2D<float4> visibilityTex = g_Textures2D[volume.visibilityTextureIndex];
			
			sum += sampleIrradianceVolume(diffuseTex, visibilityTex, g_LinearClampSampler, volume, worldSpacePos, N, V);
		}
		
		if (sum.a > 1e-5f)
		{
			result += (sum.rgb / sum.a) * albedo;
		}
	}

	// directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[g_Constants.directionalLightBufferIndex][i];
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction));
		}
	}
	
	// shadowed directional lights
	{
		for (uint i = 0; i < g_Constants.directionalLightShadowedCount; ++i)
		{
			DirectionalLight light = g_DirectionalLights[g_Constants.directionalLightShadowedBufferIndex][i];
			float shadow = getApproxShadow(worldSpacePos, light.direction);
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction)) * shadow;
		}
	}

	g_RWTextures[g_Constants.resultTextureIndex][threadID] = float4(result, 1.0f);
}