#include "bindings.hlsli"

struct PushConsts
{
	uint2 resolution;
	float2 texelSize;
	uint inputTextureIndex;
	uint historyTextureIndex;
	uint velocityTextureIndex;
	uint resultTextureIndex;
	uint ignoreHistory;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

SamplerState g_PointClampSampler : REGISTER_SAMPLER(0, 0, 1);
SamplerState g_LinearClampSampler : REGISTER_SAMPLER(1, 0, 1);

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
	
	float maxAo = -99999.0f;
	float minAo = 99999.0f;
	
	int offset = 0;
	for(int y = -2 + offset; y < 2 + offset; ++y)
	{
		for (int x = -1 - offset; x < 3 - offset; ++x)
		{
			float2 tap = g_Textures[g_PushConsts.inputTextureIndex].SampleLevel(g_PointClampSampler, g_PushConsts.texelSize * float2(x, y) + texCoord, 0.0f).xy;
			float weight = saturate(1.0f - (abs(tap.y - center.y) * rampMaxInv));
			totalAo += tap.x * weight;
			totalWeight += weight;
			
			maxAo = max(maxAo, lerp(center.x, tap.x, weight));
			minAo = min(minAo, lerp(center.x, tap.x, weight));
		}
	}
	
	float ao = totalAo / totalWeight;
	
	if (g_PushConsts.ignoreHistory == 0)
	{
		float2 velocity = g_Textures[g_PushConsts.velocityTextureIndex].Load(uint3(threadID.xy, 0)).xy;
		float2 historyTc = texCoord - velocity;
		
		if (all(historyTc > 0.0f) && all(historyTc < 1.0f))
		{
			float2 history = g_Textures[g_PushConsts.historyTextureIndex].SampleLevel(g_LinearClampSampler, historyTc, 0.0f).xy;
			history.x = clamp(history.x, minAo, maxAo);
			float weight = saturate(1.0f - (abs(history.y - center.y) * rampMaxInv));
			
			ao = lerp(ao, history.x, (23.0f / 24.0f) * weight);
		}
	}
	
	g_RWTextures[g_PushConsts.resultTextureIndex][threadID.xy] = float4(ao, center.y, 0.0f, 0.0f);
}