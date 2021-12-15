#include "bindings.hlsli"

#ifndef SKINNED
#define SKINNED 0
#endif

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
#if SKINNED
	uint4 jointIndices : JOINT_INDICES;
	float4 jointWeights : JOINT_WEIGHTS;
#endif
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	float3 worldSpacePosition : WORLD_SPACE_POS;
	float4 curScreenPos : CUR_SCREEN_POS;
	float4 prevScreenPos : PREV_SCREEN_POS;
};

struct PassConstants
{
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
};

struct DrawConstants
{
	uint transformIndex;
	uint materialIndex;
	uint entityID;
#if SKINNED
	uint skinningMatricesOffset;
#endif
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
StructuredBuffer<float4x4> g_Matrices[65536] : REGISTER_SRV(4, 1, 1);

PUSH_CONSTS(DrawConstants, g_DrawConstants);

float3x3 inverse(float3x3 m)
{
	float oneOverDeterminant = 1.0f / determinant(m);

	float3x3 result;
	result[0][0] = + (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * oneOverDeterminant;
	result[1][0] = - (m[1][0] * m[2][2] - m[2][0] * m[1][2]) * oneOverDeterminant;
	result[2][0] = + (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * oneOverDeterminant;
	result[0][1] = - (m[0][1] * m[2][2] - m[2][1] * m[0][2]) * oneOverDeterminant;
	result[1][1] = + (m[0][0] * m[2][2] - m[2][0] * m[0][2]) * oneOverDeterminant;
	result[2][1] = - (m[0][0] * m[2][1] - m[2][0] * m[0][1]) * oneOverDeterminant;
	result[0][2] = + (m[0][1] * m[1][2] - m[1][1] * m[0][2]) * oneOverDeterminant;
	result[1][2] = - (m[0][0] * m[1][2] - m[1][0] * m[0][2]) * oneOverDeterminant;
	result[2][2] = + (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * oneOverDeterminant;

	return result;
}

VSOutput main(VSInput input) 
{
	VSOutput output = (VSOutput)0;
	
	float3 vertexPos = input.position;
	float3 vertexNormal = input.normal;
	float3 vertexTangent = input.tangent.xyz;
	
	float3 prevVertexPos = input.position;
	
#if SKINNED
	float4x4 skinningMat = 0.0f;
	{
		for (uint i = 0; i < 4; ++i)
		{
			skinningMat += g_Matrices[g_PassConstants.skinningMatricesBufferIndex][g_DrawConstants.skinningMatricesOffset + input.jointIndices[i]] * input.jointWeights[i];
		}
	}
	
	vertexPos = mul(skinningMat, float4(input.position, 1.0f)).xyz;
	vertexNormal = normalize(mul(transpose(inverse((float3x3)skinningMat)), input.normal));
	vertexTangent = mul((float3x3)skinningMat, input.tangent.xyz);
	
	float4x4 prevSkinningMat = 0.0f;
	{
		for (uint i = 0; i < 4; ++i)
		{
			prevSkinningMat += g_Matrices[g_PassConstants.prevSkinningMatricesBufferIndex][g_DrawConstants.skinningMatricesOffset + input.jointIndices[i]] * input.jointWeights[i];
		}
	}
	
	prevVertexPos = mul(prevSkinningMat, float4(input.position, 1.0f)).xyz;
#endif // SKINNED

	float4x4 modelMatrix = g_Matrices[g_PassConstants.transformBufferIndex][g_DrawConstants.transformIndex];
	float3x3 normalTransform = transpose(inverse((float3x3)modelMatrix));

	output.worldSpacePosition = mul(modelMatrix, float4(vertexPos, 1.0f)).xyz;
	output.position = mul(g_PassConstants.viewProjectionMatrix, float4(output.worldSpacePosition, 1.0f));
	output.normal = normalize(mul(normalTransform, vertexNormal));
	output.tangent = float4(normalize(mul((float3x3)modelMatrix, vertexTangent)), input.tangent.w);
	output.texCoord = input.texCoord;
	
	output.curScreenPos = output.position;
	
	float4x4 prevModelMatrix = g_Matrices[g_PassConstants.prevTransformBufferIndex][g_DrawConstants.transformIndex];
	output.prevScreenPos = mul(g_PassConstants.prevViewProjectionMatrix, mul(prevModelMatrix, float4(prevVertexPos, 1.0f)));

	return output;
}