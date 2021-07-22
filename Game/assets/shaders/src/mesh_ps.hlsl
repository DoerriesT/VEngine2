#include "bindings.hlsli"

struct PSInput
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

float4 main(PSInput input) : SV_Target0
{
	float3 lightDir = normalize(float3(1.0f, 1.0f, 1.0f));
	
	float3 result = float3(0.0f, 1.0f, 0.0f) * (dot(input.normal, lightDir) * 0.5f + 0.5f);
	
	return float4(result, 1.0f);
}