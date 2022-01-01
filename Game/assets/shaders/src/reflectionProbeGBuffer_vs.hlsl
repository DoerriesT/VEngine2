#include "bindings.hlsli"

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
	uint transformBufferIndex;
	uint materialBufferIndex;
};

struct DrawConstants
{
	uint transformIndex;
	uint materialIndex;
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
	float4x4 modelMatrix = g_Matrices[g_PassConstants.transformBufferIndex][g_DrawConstants.transformIndex];
	float3x3 normalTransform = transpose(inverse((float3x3)modelMatrix));

	VSOutput output = (VSOutput)0;
	output.position = mul(g_PassConstants.viewProjectionMatrix, mul(modelMatrix, float4(input.position, 1.0f)));
	output.normal = normalize(mul(normalTransform, input.normal));
	output.tangent = float4(normalize(mul((float3x3)modelMatrix, input.tangent.xyz)), input.tangent.w);
	output.texCoord = input.texCoord;

	return output;
}