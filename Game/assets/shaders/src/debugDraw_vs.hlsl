#include "bindings.hlsli"
#include "packing.hlsli"

struct VSInput
{
	float4 positionAndColor : POSITION_AND_COLOR;
};

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);

VSOutput main(VSInput input) 
{
	VSOutput output;
	
	output.position = mul(g_PassConstants.viewProjectionMatrix, float4(input.positionAndColor.xyz, 1.0));
	output.color = unpackUnorm4x8(asuint(input.positionAndColor.w));
	
	return output;
}