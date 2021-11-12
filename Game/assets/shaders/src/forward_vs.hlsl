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
#if SKINNED
	uint skinningMatricesOffset;
#endif
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);
PUSH_CONSTS(DrawConstants, g_DrawConstants);
#if SKINNED
StructuredBuffer<float4x4> g_SkinningMatrices[65536] : REGISTER_SRV(262144, 1, 1);
#endif

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
	
#if SKINNED
	float4x4 skinningMat = 0.0f;
	for (uint i = 0; i < 4; ++i)
	{
		skinningMat += g_SkinningMatrices[g_PassConstants.skinningMatricesBufferIndex][g_DrawConstants.skinningMatricesOffset + input.jointIndices[i]] * input.jointWeights[i];
	}
	
	vertexPos = mul(skinningMat, float4(input.position, 1.0f)).xyz;
	vertexNormal = normalize(mul(transpose(inverse((float3x3)skinningMat)), input.normal));
	vertexTangent = mul((float3x3)skinningMat, input.tangent.xyz);
#endif // SKINNED

	float3x3 normalTransform = transpose(inverse((float3x3)g_DrawConstants.modelMatrix));

	output.worldSpacePosition = mul(g_DrawConstants.modelMatrix, float4(vertexPos, 1.0f)).xyz;
	output.position = mul(g_PassConstants.viewProjectionMatrix, float4(output.worldSpacePosition, 1.0f));
	output.normal = normalize(mul(normalTransform, vertexNormal));
	output.tangent = float4(normalize(mul((float3x3)g_DrawConstants.modelMatrix, vertexTangent)), input.tangent.w);
	output.texCoord = input.texCoord;

	return output;
}