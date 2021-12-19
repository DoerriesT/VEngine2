#include "bindings.hlsli"
#include "srgb.hlsli"
#include "dither.hlsli"

struct PushConsts
{
	uint4 const0;
	uint4 const1;
	float2 texelSize;
	float time;
	uint inputTextureIndex;
	uint resultTextureIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

#define A_GPU 1
#define A_HLSL 1
#include "../../../../libs/include/ffx_a.h"

AF3 CasLoad(ASU2 p)
{
	return g_Textures[g_PushConsts.inputTextureIndex].Load(int3(p, 0)).rgb;
}

void CasInput(inout AF1 r,inout AF1 g,inout AF1 b)
{
	
}

#include "../../../../libs/include/ffx_cas.h"

float4 gammaCorrectAndDither(float3 c, uint2 coord)
{
	c = accurateLinearToSRGB(c);
	c = ditherTriangleNoise(c, (coord + 0.5f) * g_PushConsts.texelSize, g_PushConsts.time);
	
	return float4(c, 1.0f);
}

[numthreads(64, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	AU2 gxy = ARmp8x8(groupThreadID.x) + AU2(groupID.x << 4u, groupID.y << 4u);
	
	const uint4 const0 = g_PushConsts.const0;
	const uint4 const1 = g_PushConsts.const1;
	
	AF3 c;
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_RWTextures[g_PushConsts.resultTextureIndex][gxy] = gammaCorrectAndDither(c, gxy);
	gxy.x += 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_RWTextures[g_PushConsts.resultTextureIndex][gxy] = gammaCorrectAndDither(c, gxy);
	gxy.y += 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_RWTextures[g_PushConsts.resultTextureIndex][gxy] = gammaCorrectAndDither(c, gxy);
	gxy.x -= 8u;
	
	CasFilter(c.r, c.g, c.b, gxy, const0, const1, true);
	g_RWTextures[g_PushConsts.resultTextureIndex][gxy] = gammaCorrectAndDither(c, gxy);
}