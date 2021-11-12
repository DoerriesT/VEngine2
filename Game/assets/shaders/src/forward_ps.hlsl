#include "bindings.hlsli"
#include "shadingModel.hlsli"
#include "packing.hlsli"
#include "srgb.hlsli"
#include "material.hlsli"

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
};

struct DrawConstants
{
	float4x4 modelMatrix;
	uint materialIndex;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<Material> g_Materials[65536] : REGISTER_SRV(262144, 2, 1);
SamplerState g_AnisoRepeatSampler : REGISTER_SAMPLER(0, 0, 2);
PUSH_CONSTS(DrawConstants, g_DrawConstants);

PSOutput main(PSInput input)
{
	float3 lightDir = normalize(float3(1.0f, 1.0f, 1.0f));
	float3 lightIntensity = 3.0f;
	
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
	
	float3 result = 0.0f;
	
	// directional light
	{
		result += Default_Lit(albedo, F0, lightIntensity, N, V, lightDir, roughness, metalness);
	}
	
	// ambient light
	{
		result += 0.5f * albedo;
	}
	
	PSOutput output = (PSOutput)0;
	output.color = float4(result, 1.0f);
	output.normalRoughness = float4(encodeOctahedron24(N), roughness);
	output.albedoMetalness = float4(albedo, metalness);
	
	return output;
}