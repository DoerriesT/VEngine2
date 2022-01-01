#include "bindings.hlsli"
#include "packing.hlsli"
#include "lightEvaluation.hlsli"
#include "srgb.hlsli"

struct Constants
{
	float4x4 probeFaceToWorldSpace[6];
	float3 probePosition;
	float texelSize;
	uint resolution;
	uint directionalLightCount;
	uint directionalLightBufferIndex;
	uint directionalLightShadowedCount;
	uint directionalLightShadowedBufferIndex;
	uint albedoRoughnessTextureIndex;
	uint normalDepthTextureIndex;
	uint resultTextureIndex;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2DArray<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<DirectionalLight> g_DirectionalLights[65536] : REGISTER_SRV(4, 1, 1);
RWTexture2DArray<float4> g_RWTextures[65536] : REGISTER_UAV(1, 2, 1);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_Constants.resolution))
	{
		return;
	}
	
	float3 normalDepth = g_Textures[g_Constants.normalDepthTextureIndex].Load(int4(threadID, 0)).xyz;
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
	float4 albedoRoughness = g_Textures[g_Constants.albedoRoughnessTextureIndex].Load(int4(threadID, 0));
	float3 albedo = accurateSRGBToLinear(albedoRoughness.rgb);
	float roughness = albedoRoughness.w;
	
	float3 result = 0.0f;
	
	result += Diffuse_Lambert(albedo);

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
			result += Diffuse_Lambert(albedo) * light.color * saturate(dot(N, light.direction));
		}
	}

	g_RWTextures[g_Constants.resultTextureIndex][threadID] = float4(result, 1.0f);
}