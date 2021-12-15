#include "bindings.hlsli"

struct Constants
{
	uint width;
	uint height;
	float bicubicSharpness;
	float jitterOffsetWeight;
	uint ignoreHistory;
	uint resultTextureIndex;
	uint inputTextureIndex;
	uint historyTextureIndex;
	uint depthTextureIndex;
	uint velocityTextureIndex;
	uint exposureDataBufferIndex;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);
Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 1, 1);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 0, 1);
SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

float3 clipAABB(float3 p, float3 aabbMin, float3 aabbMax)
{
    //Clips towards AABB center for better perfomance
    float3 center   = 0.5 * (aabbMax + aabbMin);
    float3 halfSize = 0.5 * (aabbMax - aabbMin) + 1e-5;
    //Relative position from the center
    float3 clip     = p - center;
    //Normalize relative position
    float3 unit     = clip / halfSize;
    float3 absUnit  = abs(unit);
    float maxUnit = max(absUnit.x, max(absUnit.y, absUnit.z));
	
	return (maxUnit > 1.0) ? clip * (1.0 / maxUnit) + center : p;
}

float2 weightedLerpFactors(float weightA, float weightB, float blend)
{
	float blendA = (1.0 - blend) * weightA;
	float blendB = blend * weightB;
	float invBlend = 1.0 / (blendA + blendB);
	blendA *= invBlend;
	blendB *= invBlend;
	return float2(blendA, blendB);
}

float3 simpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	return color / max(1.0 + luma, 1e-6);
}

float3 inverseSimpleTonemap(float3 color)
{
	float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	return color / max(1.0 - luma, 1e-6);
}

float3 rgbToYcocg(float3 color)
{
	return color;
	float3 result;
	result.x = dot(color, float3(  1, 2,  1 ) );
	result.y = dot(color, float3(  2, 0, -2 ) );
	result.z = dot(color, float3( -1, 2, -1 ) );
	
	return result;
}

float3 ycocgToRgb(float3 color )
{
	return color;
	float y  = color.x * 0.25f;
	float co = color.y * 0.25f;
	float cg = color.z * 0.25f;

	float3 result;
	result.r = y + co - cg;
	result.g = y + cg;
	result.b = y - co - cg;

	return result;
}

float3 sampleHistory(float2 texCoord, float4 rtMetrics)
{
	const float sharpening = g_Constants.bicubicSharpness;  // [0.0, 1.0]
	
	float2 samplePos = texCoord * rtMetrics.xy;
	float2 tc1 = floor(samplePos - 0.5f) + 0.5f;
	float2 f = samplePos - tc1;
	float2 f2 = f * f;
	float2 f3 = f * f2;
	
	// Catmull-Rom weights
	const float c = sharpening;
	float2 w0 = -(c)        * f3 + (2.0f * c)        * f2 - (c * f);
	float2 w1 =  (2.0f - c) * f3 - (3.0f - c)        * f2            + 1.0f;
	float2 w2 = -(2.0f - c) * f3 + (3.0f -  2.0f * c) * f2 + (c * f);
	float2 w3 =  (c)        * f3 - (c)              * f2;
	
	float2 w12  = w1 + w2;
	float2 tc0  = (tc1 - 1.0)      * rtMetrics.zw;
	float2 tc3  = (tc1 + 2.0)      * rtMetrics.zw;
	float2 tc12 = (tc1 + w2 / w12) * rtMetrics.zw;
	
	// Bicubic filter using bilinear lookups, skipping the 4 corner texels
	float4 filtered = float4(g_Textures[g_Constants.historyTextureIndex].SampleLevel(g_LinearClampSampler, float2(tc12.x, tc0.y ), 0.0f).rgb, 1.0f) * (w12.x *  w0.y) +
	                  float4(g_Textures[g_Constants.historyTextureIndex].SampleLevel(g_LinearClampSampler, float2(tc0.x,  tc12.y), 0.0f).rgb, 1.0f) * ( w0.x * w12.y) +
	                  float4(g_Textures[g_Constants.historyTextureIndex].SampleLevel(g_LinearClampSampler, float2(tc12.x, tc12.y), 0.0f).rgb, 1.0f) * (w12.x * w12.y) +  // Center pixel
	                  float4(g_Textures[g_Constants.historyTextureIndex].SampleLevel(g_LinearClampSampler, float2(tc3.x,  tc12.y), 0.0f).rgb, 1.0f) * ( w3.x * w12.y) +
	                  float4(g_Textures[g_Constants.historyTextureIndex].SampleLevel(g_LinearClampSampler, float2(tc12.x, tc3.y ), 0.0f).rgb, 1.0f) * (w12.x *  w3.y);
	
	return filtered.rgb * (1.0 / filtered.a);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_Constants.width || threadID.y >= g_Constants.height)
	{
		return;
	}
	
	if (g_Constants.ignoreHistory != 0)
	{
		g_RWTextures[g_Constants.resultTextureIndex][threadID.xy] = float4(g_Textures[g_Constants.inputTextureIndex].Load(int3(threadID.xy, 0)).rgb, 1.0f);
		return;
	}
	
	// find frontmost velocity in neighborhood
	int2 velocityCoordOffset = 0;
	{
		float closestDepth = 0.0;
		for (int y = -1; y < 2; ++y)
		{
			for (int x = -1; x < 2; ++x)
			{
				float depth = g_Textures[g_Constants.depthTextureIndex].Load(int3(threadID.xy + int2(x,y), 0)).x;
				velocityCoordOffset = depth > closestDepth ? int2(x, y) : velocityCoordOffset;
				closestDepth = depth > closestDepth ? depth : closestDepth;
			}
		}
	}
	
	
	float2 velocity = g_Textures[g_Constants.velocityTextureIndex].Load(int3(threadID.xy + velocityCoordOffset, 0)).xy;
	
	float3 currentColor = rgbToYcocg(simpleTonemap(g_Textures[g_Constants.inputTextureIndex].Load(int3(threadID.xy, 0)).rgb));
	float3 neighborhoodMin = currentColor;
	float3 neighborhoodMax = currentColor;
	float3 neighborhoodMinPlus = currentColor;
	float3 neighborhoodMaxPlus = currentColor;
	
	{
		float3 m1 = 0.0f;
		float3 m2 = 0.0f;
		for (int y = 0; y < 3; ++y)
		{
			for (int x = 0; x < 3; ++x)
			{
				float3 tap = rgbToYcocg(simpleTonemap(g_Textures[g_Constants.inputTextureIndex].Load(int3(threadID.xy + int2(x, y) - 1, 0)).rgb));
				
				m1 += tap;
				m2 += tap * tap;
			}
		}	
		
		float3 mean = m1 * (1.0f / 9.0f);
		float3 stddev = sqrt(max((m2  * (1.0f / 9.0f) - mean * mean), 1e-7f));
		
		float wideningFactor = 1.0f;
		
		neighborhoodMin = -stddev * wideningFactor + mean;
		neighborhoodMax = stddev * wideningFactor + mean;
	}
	
	float2 texSize = float2(g_Constants.width, g_Constants.height);
	float2 texelSize = 1.0f / texSize;
	float2 texCoord = (float2(threadID.xy) + 0.5f) * texelSize;
	float2 prevTexCoord = texCoord - velocity;
	
	// history is pre-exposed -> convert from previous frame exposure to current frame exposure
	float exposureConversionFactor = asfloat(g_ByteAddressBuffers[g_Constants.exposureDataBufferIndex].Load(1 << 2)); // 0 = current frame exposure | 1 = previous frame to current frame exposure
	
	float3 historyColor = rgbToYcocg(simpleTonemap(max(sampleHistory(prevTexCoord, float4(texSize, texelSize)).rgb, 0.0f) * exposureConversionFactor));
	historyColor = clipAABB(historyColor, neighborhoodMin, neighborhoodMax);
	
	float subpixelCorrection = abs(frac(max(abs(prevTexCoord.x) * texSize.x, abs(prevTexCoord.y) * texSize.y)) * 2.0f - 1.0f);
	
	float alpha = 0.05f;//lerp(0.05f, 0.8f, subpixelCorrection);
	//alpha *= g_Constants.jitterOffsetWeight;
	alpha = prevTexCoord.x <= 0.0f || prevTexCoord.x >= 1.0f || prevTexCoord.y <= 0.0f || prevTexCoord.y >= 1.0f ? 1.0f : alpha;
	
	float2 factors = weightedLerpFactors(1.0f / (historyColor.x + 4.0f), 1.0f / (currentColor.x + 4.0f), alpha);
	
	float3 result = lerp(historyColor, currentColor, alpha);//historyColor * factors.x + currentColor * factors.y;
	
	g_RWTextures[g_Constants.resultTextureIndex][threadID.xy] = float4(inverseSimpleTonemap(ycocgToRgb(result)), 1.0f);
}