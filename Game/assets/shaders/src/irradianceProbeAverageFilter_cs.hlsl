#include "bindings.hlsli"

struct PushConsts
{
	uint2 resolution;
	uint inputTextureIndex;
	uint resultTextureIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 1, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_PushConsts.resolution))
	{
		return;
	}
	
	const uint diffuseProbeRes = 8;
	uint2 baseOffset = threadID.xy * (diffuseProbeRes + 2);
	
	float3 sum = 0.0f;
	for (uint y = 0; y < diffuseProbeRes; ++y)
	{
		for (int x = 0; x < diffuseProbeRes; ++x)
		{
			sum += g_Textures[g_PushConsts.inputTextureIndex].Load(int3(baseOffset + uint2(x, y), 0)).rgb;
		}
	}
	
	g_RWTextures[g_PushConsts.resultTextureIndex][threadID.xy] = float4(sum / (diffuseProbeRes * diffuseProbeRes), 1.0f);
}