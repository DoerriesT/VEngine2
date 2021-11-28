#include "bindings.hlsli"

struct PassConstants
{
	uint tileCountX;
	uint transformBufferIndex;
	uint wordCount;
	uint resultTextureIndex;
};

struct PushConsts
{
	uint lightIndex;
	uint transformIndex;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);

StructuredBuffer<float4x4> g_Transforms[65536] : REGISTER_SRV(4, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float4 main(float3 position : POSITION) : SV_Position
{
	return mul(g_Transforms[g_PassConstants.transformBufferIndex][g_PushConsts.transformIndex], float4(position * 1.1, 1.0));
}