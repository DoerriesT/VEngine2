#include "bindings.hlsli"

struct PushConsts
{
	uint2 resolution;
	float2 texelSize;
	uint inputTextureIndex;
	uint resultTextureIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);
SamplerState g_PointClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_PushConsts.resolution))
	{
		return;
	}

	float2 texCoord = float2(threadID.xy + 0.5f) * g_PushConsts.texelSize;
	float2 center = g_Textures[g_PushConsts.inputTextureIndex].Load(int3(threadID.xy, 0)).xy;
	float rampMaxInv = 1.0f / (center.y * 0.1f);
	
	float totalAo = 0.0f;
	float totalWeight = 0.0f;
	
	int offset = 0;//uFrame % 2;
	for(int y = -2 + offset; y < 2 + offset; ++y)
	{
		for (int x = -1 - offset; x < 3 - offset; ++x)
		{
			float2 tap = g_Textures[g_PushConsts.inputTextureIndex].SampleLevel(g_PointClampSampler, g_PushConsts.texelSize * float2(x, y) + texCoord, 0.0f).xy;
			float weight = saturate(1.0f - (abs(tap.y - center.y) * rampMaxInv));
			totalAo += tap.x * weight;
			totalWeight += weight;
		}
	}
	
	float ao = totalAo / totalWeight;
	
	g_RWTextures[g_PushConsts.resultTextureIndex][threadID.xy] = float4(ao, center.y, 0.0f, 0.0f);
}