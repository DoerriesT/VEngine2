#include "bindings.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
SamplerState g_AnisoRepeatSampler : REGISTER_SAMPLER(0, 0, 2);

void main(PSInput input)
{
	if (input.textureIndex != 0)
	{
		float alpha = g_Textures[input.textureIndex].Sample(g_AnisoRepeatSampler, input.texCoord).a;
		if (alpha < 0.5f)
		{
			discard;
		}
	}
}