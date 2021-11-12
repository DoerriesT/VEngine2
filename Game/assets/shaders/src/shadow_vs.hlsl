#include "bindings.hlsli"
#include "material.hlsli"

#ifndef SKINNED
#define SKINNED 0
#endif

#ifndef ALPHA_TESTED
#define ALPHA_TESTED 0
#endif

struct VSInput
{
	float3 position : POSITION;
#if ALPHA_TESTED
	float2 texCoord : TEXCOORD;
#endif
#if SKINNED
	uint4 jointIndices : JOINT_INDICES;
	float4 jointWeights : JOINT_WEIGHTS;
#endif
};

struct VSOutput
{
	float4 position : SV_Position;
#if ALPHA_TESTED
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
#endif
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
	uint skinningMatricesBufferIndex;
	uint materialBufferIndex;
};

struct DrawConstants
{
	float4x4 modelMatrix;
#if SKINNED
	uint skinningMatricesOffset;
#endif
#if ALPHA_TESTED
	uint materialIndex;
#endif
};



ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
#if SKINNED
StructuredBuffer<float4x4> g_SkinningMatrices[65536] : REGISTER_SRV(262144, 1, 1);
#endif
#if ALPHA_TESTED
StructuredBuffer<Material> g_Materials[65536] : REGISTER_SRV(262144, 2, 1);
#endif

PUSH_CONSTS(DrawConstants, g_DrawConstants);

VSOutput main(VSInput input) 
{
	VSOutput output = (VSOutput)0;
	
	float3 vertexPos = input.position;
	
#if SKINNED
	float4x4 skinningMat = 0.0f;
	for (uint i = 0; i < 4; ++i)
	{
		skinningMat += g_SkinningMatrices[g_PassConstants.skinningMatricesBufferIndex][g_DrawConstants.skinningMatricesOffset + input.jointIndices[i]] * input.jointWeights[i];
	}
	
	vertexPos = mul(skinningMat, float4(input.position, 1.0f)).xyz;
#endif

	output.position = mul(g_PassConstants.viewProjectionMatrix, mul(g_DrawConstants.modelMatrix, float4(vertexPos, 1.0f)));

#if ALPHA_TESTED
	Material material = g_Materials[g_PassConstants.materialBufferIndex][g_DrawConstants.materialIndex];
	output.textureIndex = material.albedoTextureHandle;
	output.texCoord = input.texCoord;
#endif

	return output;
}