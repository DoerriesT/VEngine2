#include "bindings.hlsli"

#ifndef ALPHA_TESTED
#define ALPHA_TESTED 0
#endif

struct PSInput
{
	float4 position : SV_Position;
	nointerpolation uint outlineID : OUTLINE_ID;
#if ALPHA_TESTED
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
#endif
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
SamplerState g_AnisoRepeatSampler : REGISTER_SAMPLER(0, 0, 2);

float main(PSInput input) : SV_Target0
{
#if ALPHA_TESTED
	if (input.textureIndex != 0)
	{
		float alpha = g_Textures[input.textureIndex].Sample(g_AnisoRepeatSampler, input.texCoord).a;
		if (alpha < 0.5f)
		{
			discard;
		}
	}
#endif
	return input.outlineID / 255.0f;
}