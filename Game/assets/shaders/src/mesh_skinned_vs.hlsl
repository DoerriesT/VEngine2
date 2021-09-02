#include "bindings.hlsli"

struct VSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	uint4 jointIndices : JOINT_INDICES;
	float4 jointWeights : JOINT_WEIGHTS;
};

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texCoord : TEXCOORD;
	float3 worldSpacePosition : WORLD_SPACE_POS;
};

struct Constants
{
	float4x4 modelMatrix;
	float4x4 viewProjectionMatrix;
	uint skinningMatricesBufferIndex;
	uint skinningMatricesOffset;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
StructuredBuffer<float4x4> g_SkinningMatrices[65536] : REGISTER_SRV(262144, 1, 1);

VSOutput main(VSInput input) 
{
	VSOutput output = (VSOutput)0;
	
	float3 vertexPos = 0.0f;
	float3 vertexNormal = 0.0f;
	float3 vertexTangent = 0.0f;
	
	for (uint i = 0; i < 4; ++i)
	{
		float4x4 skinningMat = g_SkinningMatrices[g_Constants.skinningMatricesBufferIndex][g_Constants.skinningMatricesOffset + input.jointIndices[i]];
		vertexPos += mul(skinningMat, float4(input.position, 1.0f)).xyz * input.jointWeights[i];
		vertexNormal += mul((float3x3)skinningMat, input.normal) * input.jointWeights[i];
		vertexTangent += mul((float3x3)skinningMat, input.tangent.xyz) * input.jointWeights[i];
	}
	
	vertexNormal = normalize(vertexNormal);
	vertexTangent = normalize(vertexTangent);

	output.worldSpacePosition = mul(g_Constants.modelMatrix, float4(vertexPos, 1.0f)).xyz;
	output.position = mul(g_Constants.viewProjectionMatrix, float4(output.worldSpacePosition, 1.0f));
	output.normal = normalize(mul((float3x3)g_Constants.modelMatrix, vertexNormal));
	output.tangent = float4(normalize(mul((float3x3)g_Constants.modelMatrix, vertexTangent)), input.tangent.w);
	output.texCoord = input.texCoord;

	return output;
}