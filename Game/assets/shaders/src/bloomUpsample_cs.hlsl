#include "bindings.hlsli"

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
	uint addPrevious;
	uint inputImageIndex;
	uint prevResultImageIndex;
	uint resultImageIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	const float2 texelSize = g_PushConsts.texelSize;
	const float2 texCoord = float2(threadID.xy + 0.5f) * texelSize;
	
	const float radius = 1.7f;
	
	float3 sum = 0.0f;
	
	Texture2D<float4> inputImage = g_Textures[g_PushConsts.inputImageIndex];
	
	sum += 1.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x, 	-texelSize.y) * radius, 0.0f).rgb;
	sum += 2.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( 0.0f,         -texelSize.y) * radius, 0.0f).rgb;
	sum += 1.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x, 	-texelSize.y) * radius, 0.0f).rgb;
	
	sum += 2.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x,          0.0f) * radius, 0.0f).rgb;
	sum += 4.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord,                                               	0.0f).rgb;
	sum += 2.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x,          0.0f) * radius, 0.0f).rgb;
	
	sum += 1.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x,   texelSize.y) * radius, 0.0f).rgb;
	sum += 2.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( 0.0,           texelSize.y) * radius, 0.0f).rgb;
	sum += 1.0f * inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x,   texelSize.y) * radius, 0.0f).rgb;
	
	sum *= (1.0f / 16.0f);
	
	if (g_PushConsts.addPrevious != 0)
	{
		sum += g_Textures[g_PushConsts.prevResultImageIndex].SampleLevel(g_LinearClampSampler, texCoord, 0.0).rgb;
		sum *= 0.5f;
	}
	
	g_RWTextures[g_PushConsts.resultImageIndex][threadID.xy] = float4(sum, 1.0f);
}