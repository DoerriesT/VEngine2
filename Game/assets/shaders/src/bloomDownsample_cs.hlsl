#include "bindings.hlsli"

struct PushConsts
{
	float2 texelSize;
	uint width;
	uint height;
	uint doWeightedAverage;
	uint inputImageIndex;
	uint resultImageIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float4 addTap(float3 tap, float weight, bool lumaWeighted)
{
	weight *= lumaWeighted ? 1.0f / (1.0f + dot(tap, float3(0.2126f, 0.7152f, 0.0722f))) : 1.0f;
	return float4(weight * tap, weight);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.width || threadID.y >= g_PushConsts.height)
	{
		return;
	}
	
	const float2 texelSize = g_PushConsts.texelSize;
	const float2 texCoord = float2(threadID.xy + 0.5f) * texelSize;
	
	float4 sum = 0.0f;
	
	const bool doWeightedAverage = (g_PushConsts.doWeightedAverage != 0);
	
	Texture2D<float4> inputImage = g_Textures[g_PushConsts.inputImageIndex];
	
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x, 	-texelSize.y),			0.0f).rgb, 0.125f, doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( 0.0f,         -texelSize.y),			0.0f).rgb, 0.25f,  doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x, 	-texelSize.y),			0.0f).rgb, 0.125f, doWeightedAverage);
	
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x, 	-texelSize.y) * 0.5f,	0.0f).rgb, 0.5f,   doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x, 	-texelSize.y) * 0.5f,	0.0f).rgb, 0.5f,   doWeightedAverage);
	
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x,          0.0f),			0.0f).rgb, 0.25f,  doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord,                                      			0.0f).rgb, 0.5f,   doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x,          0.0f),			0.0f).rgb, 0.25f,  doWeightedAverage);
	
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x,   texelSize.y) * 0.5f,	0.0f).rgb, 0.5f,   doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x,   texelSize.y) * 0.5f,	0.0f).rgb, 0.5f,   doWeightedAverage);
	
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2(-texelSize.x,	 texelSize.y), 			0.0f).rgb, 0.125f, doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( 0.0f,          texelSize.y),			0.0f).rgb, 0.25f,  doWeightedAverage);
	sum += addTap(inputImage.SampleLevel(g_LinearClampSampler, texCoord + float2( texelSize.x,   texelSize.y), 			0.0f).rgb, 0.125f, doWeightedAverage);
	
	sum.rgb /= sum.a;
	
	g_RWTextures[g_PushConsts.resultImageIndex][threadID.xy] = float4(sum.rgb, 1.0f);
}