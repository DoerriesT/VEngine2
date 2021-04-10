#include "bindings.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 texCoord : TEXCOORD0;
	nointerpolation uint textureIndex : TEXTURE_INDEX;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
SamplerState g_LinearRepeatSampler : REGISTER_SAMPLER(0, 0, 1);

float4 main(PSInput input) : SV_Target0
{
	return input.color * g_Textures[input.textureIndex].Sample(g_LinearRepeatSampler, input.texCoord);
}