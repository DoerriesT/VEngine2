#include "bindings.hlsli"
#include "srgb.hlsli"

struct PushConsts
{
	uint2 resolution;
	float2 texelSize;
	float time;
	uint applyBloom;
	float bloomStrength;
	uint inputImageIndex;
	uint bloomImageIndex;
	uint exposureBufferIndex;
	uint outputImageIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 1, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

// n must be normalized in [0..1] (e.g. texture coordinates)
float triangleNoise(float2 n)
{
    // triangle noise, in [-1.0..1.0[ range
    n  = frac(n * float2(5.3987, 5.4421));
    n += dot(n.yx, n.xy + float2(21.5351, 14.3137));

    float xy = n.x * n.y;
    // compute in [0..2[ and remap to [-1.0..1.0[
    return frac(xy * 95.4307) + frac(xy * 75.04961) - 1.0;
}

float3 ditherTriangleNoise(float3 rgb, float2 uv, float time)
{
    // Gjøl 2016, "Banding in Games: A Noisy Rant"
    uv += 0.07 * frac(time);

    float noise = triangleNoise(uv);

    // noise is in [-1..1[

    return rgb + (noise / 255.0);
}

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
float3 uchimura(float3 x, float P, float a, float m, float l, float c, float b)
{
	float l0 = ((P - m) * l) / a;
	float L0 = m - m / a;
	float L1 = m + (1.0f - m) / a;
	float S0 = m + l0;
	float S1 = m + a * l0;
	float C2 = (a * P) / (P - S1);
	float CP = -C2 / P;
	
	float3 w0 = 1.0f - smoothstep(0.0f, m, x);
	float3 w2 = step(m + l0, x);
	float3 w1 = 1.0f - w0 - w2;
	
	float3 T = m * pow(x / m, c) + b;
	float3 S = P - (P - S1) * exp(CP * (x - S0));
	float3 L = m + a * (x - m);
	
	return T * w0 + L * w1 + S * w2;
}

float3 uchimura(float3 x)
{
	const float P = 1.0;  // max display brightness
	const float a = 1.0;  // contrast
	const float m = 0.22; // linear section start
	const float l = 0.4;  // linear section length
	const float c = 1.33; // black
	const float b = 0.0;  // pedestal
	
	return uchimura(x, P, a, m, l, c, b);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (any(threadID.xy >= g_PushConsts.resolution))
	{
		return;
	}
	
	const float2 texCoord = float2(float2(threadID.xy) + 0.5) * g_PushConsts.texelSize;
	
	float3 color = g_Textures[g_PushConsts.inputImageIndex].Load(int3(threadID.xy, 0)).rgb;
	
	if (g_PushConsts.applyBloom)
	{
		float3 bloom = g_Textures[g_PushConsts.bloomImageIndex].SampleLevel(g_LinearClampSampler, texCoord, 0.0f).rgb;
		color = lerp(color, bloom, g_PushConsts.bloomStrength);
	}

	const float prevToCurExposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(4));
	color *= prevToCurExposure; 
	color = uchimura(color);
	color = accurateLinearToSRGB(color);
	color = ditherTriangleNoise(color, texCoord, g_PushConsts.time);
	
	g_RWTextures[g_PushConsts.outputImageIndex][threadID.xy] = float4(color, 1.0f);
}