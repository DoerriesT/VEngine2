#include "bindings.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
};

struct PushConsts
{
	float4x4 viewProjection;
	float3 worldSpacePosition;
	uint irradianceVolumeTextureIndex;
	uint3 probeIdx;
	uint exposureBufferIndex;
};

PUSH_CONSTS(PushConsts, g_PushConsts);

VSOutput main(float3 position : POSITION)
{
	VSOutput output = (VSOutput)0;
	output.position = mul(g_PushConsts.viewProjection, float4(g_PushConsts.worldSpacePosition + (position * 0.1f), 1.0));
	output.normal = position;
	return output;
}