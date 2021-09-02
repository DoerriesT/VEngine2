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
	float3 worldSpacePosition : WORLD_SPACE_POS;
};

struct Constants
{
	float4x4 modelMatrix;
	float4x4 viewProjectionMatrix;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);

VSOutput main(VSInput input) 
{
	VSOutput output = (VSOutput)0;

	output.worldSpacePosition = mul(g_Constants.modelMatrix, float4(input.position, 1.0f)).xyz;
	output.position = mul(g_Constants.viewProjectionMatrix, float4(output.worldSpacePosition, 1.0f));
	output.normal = normalize(mul((float3x3)g_Constants.modelMatrix, input.normal));
	output.tangent = float4(normalize(mul((float3x3)g_Constants.modelMatrix, input.tangent.xyz)), input.tangent.w);
	output.texCoord = input.texCoord;

	return output;
}