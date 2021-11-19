#include "bindings.hlsli"


struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

struct PushConsts
{
	float3 outlineColor;
	uint outlineIDTextureIndex;
	uint outlineDepthTextureIndex;
	uint sceneDepthTextureIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);
PUSH_CONSTS(PushConsts, g_PushConsts);

// https://ourmachinery.com/post/borderland-part-3-selection-highlighting/

float4 main(PSInput input) : SV_Target0
{
	Texture2D<float4> idTex = g_Textures[g_PushConsts.outlineIDTextureIndex];
	float4 tap0 = idTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(-2, -2)) * 255.0f;
	float4 tap1 = idTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(0, -2)) * 255.0f;
	float4 tap2 = idTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(-2, 0)) * 255.0f;
	float4 tap3 = idTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(0, 0)) * 255.0f;

	// throw away unused taps and merge into two float4 and a center tap
	tap2.xw = tap1.xy;
	float center = tap3.w;
	tap3.w = tap0.y;
	float alpha = dot(float4(tap2 != center), 1.0f / 8.0f);
	alpha += dot(float4(tap3 != center), 1.0f / 8.0f);
	
	if (alpha == 0.0f)
	{
		discard;
	}
	
	alpha = 1.0f;
	
	// find closest depth in neighborhood
	float closestDepth = 0.0f;
	
	Texture2D<float4> outlineDepthTex = g_Textures[g_PushConsts.outlineDepthTextureIndex];
	
	float4 dtap0 = outlineDepthTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(-2, -2));
	closestDepth = max(closestDepth, max(max(dtap0.x, dtap0.y), max(dtap0.z, dtap0.w)));
	
	float4 dtap1 = outlineDepthTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(0, -2));
	closestDepth = max(closestDepth, max(max(dtap1.x, dtap1.y), max(dtap1.z, dtap1.w)));
	
	float4 dtap2 = outlineDepthTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(-2, 0));
	closestDepth = max(closestDepth, max(max(dtap2.x, dtap2.y), max(dtap2.z, dtap2.w)));
	
	float4 dtap3 = outlineDepthTex.GatherRed(g_LinearClampSampler, input.texCoord, int2(0, 0));
	closestDepth = max(closestDepth, max(max(dtap3.x, dtap3.y), max(dtap3.z, dtap3.w)));

	float sceneDepth = g_Textures[g_PushConsts.sceneDepthTextureIndex].Load(uint3(input.position.xy, 0)).x;
	
	bool visible = closestDepth >= sceneDepth;
	float3 color = g_PushConsts.outlineColor;
	color *= visible ? 1.0f : 0.3f;
	
	return float4(color, alpha);
}