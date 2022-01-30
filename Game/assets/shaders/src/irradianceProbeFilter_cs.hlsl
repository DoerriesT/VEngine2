#include "bindings.hlsli"
#include "packing.hlsli"
#include "brdf.hlsli"

#ifndef OUTPUT_VISIBILITY
#define OUTPUT_VISIBILITY 0
#endif

#define DIFFUSE_PROBE_RESOLUTION 8
#define VISIBILITY_PROBE_RESOLUTION 16

#if !OUTPUT_VISIBILITY
#define WORK_GROUP_SIZE DIFFUSE_PROBE_RESOLUTION
#define PROBE_RESOLUTION DIFFUSE_PROBE_RESOLUTION
#else
#define WORK_GROUP_SIZE VISIBILITY_PROBE_RESOLUTION
#define PROBE_RESOLUTION VISIBILITY_PROBE_RESOLUTION
#endif // !OUTPUT_VISIBILITY

struct PushConsts
{
	uint2 baseOutputOffset;
	float texelSize;
	float maxDistance;
	uint inputTextureIndex;
	uint resultTextureIndex;
};

TextureCube<float4> g_CubeTextures[65536] : REGISTER_SRV(0, 0, 0);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 1, 0);

SamplerState g_PointClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 getCubeDir(float2 uv, int face)
{
	uv = uv * 2.0f - 1.0f;
	uv.y = -uv.y;

	switch (face)
	{
	case 0:
		return float3(1.0f, uv.y, -uv.x);
	case 1:
		return float3(-1.0f, uv.y, uv.x);
	case 2:
		return float3(uv.x, 1.0f, -uv.y);
	case 3:
		return float3(uv.x, -1.0f, uv.y);
	case 4:
		return float3(uv.x, uv.y, 1.0f);
	default:
		return float3(-uv.x, uv.y, -1.0f);
	}
}

[numthreads(WORK_GROUP_SIZE, WORK_GROUP_SIZE, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
	const float epsilon = 1e-6f;
	const float texelSize = g_PushConsts.texelSize;
	
	const float3 texelDir = decodeOctahedron(((groupThreadID.xy + 0.5f) / float(PROBE_RESOLUTION)) * 2.0f - 1.0f);
	
	float4 result = 0.0f;
	
	for (int face = 0; face < 6; ++face)
	{
		for (float v = (texelSize * 0.5f); v < 1.0f; v += texelSize)
		{
			for (float u = (texelSize * 0.5f); u < 1.0f; u += texelSize)
			{
				const float3 sampleDir = normalize(getCubeDir(float2(u, v), face));
				const float texelDotSample = dot(texelDir, sampleDir);
				
				if (texelDotSample <= 0.0f)
				{
					continue;
				}
				
#if !OUTPUT_VISIBILITY
				const float weight = saturate(texelDotSample);
				if (weight >= epsilon)
				{
					float3 tap = g_CubeTextures[g_PushConsts.inputTextureIndex].SampleLevel(g_PointClampSampler, sampleDir, 0.0f).rgb;
					result += float4(tap * weight, weight);
				}
#else
				const float weight = pow(saturate(texelDotSample), 50.0f);
				if (weight >= epsilon)
				{
					float tap = g_CubeTextures[g_PushConsts.inputTextureIndex].SampleLevel(g_PointClampSampler, sampleDir, 0.0f).x;
					tap = min(tap, g_PushConsts.maxDistance);
					result += float4(tap * weight, tap * tap * weight, 0.0f, weight);
				}
#endif
			}
		}
	}
	
	if (result.w >= epsilon)
	{
		result /= result.w;
		
#if !OUTPUT_VISIBILITY
		result.rgb *= 2.0f * PI; // divide by uniform hemisphere sampling pdf (1.0f / (2.0f * PI))
#endif
		
#if OUTPUT_VISIBILITY
		result.zw = 0.0f; // hint to the compiler that we only care about the xy components
#endif
		
		uint2 outputCoord = groupID.xy * (PROBE_RESOLUTION + 2) + g_PushConsts.baseOutputOffset + groupThreadID.xy + 1;
		
		g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord] = float4(result.rgb, 1.0f);
		
		const int2 offset = (int2)groupThreadID.xy * -2 + (PROBE_RESOLUTION - 1);
		
		// left border
		if (groupThreadID.x == 0)
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(-1, offset.y)] = float4(result.rgb, 1.0f);
		}
		// right border
		else if (groupThreadID.x == (PROBE_RESOLUTION - 1))
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(1, offset.y)] = float4(result.rgb, 1.0f);
		}
		
		// top border
		if (groupThreadID.y == 0)
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(offset.x, -1)] = float4(result.rgb, 1.0f);
		}
		// bottom border
		else if (groupThreadID.y == (PROBE_RESOLUTION - 1))
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(offset.x, 1)] = float4(result.rgb, 1.0f);
		}
		
		// top left corner
		if (groupThreadID.x == 0 && groupThreadID.y == 0)
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(PROBE_RESOLUTION, PROBE_RESOLUTION)] = float4(result.rgb, 1.0f);
		}
		// bottom left corner
		else if (groupThreadID.x == 0 && groupThreadID.y == (PROBE_RESOLUTION - 1))
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(PROBE_RESOLUTION, -PROBE_RESOLUTION)] = float4(result.rgb, 1.0f);
		}
		// top right
		else if (groupThreadID.x == (PROBE_RESOLUTION - 1) && groupThreadID.y == 0)
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(-PROBE_RESOLUTION, PROBE_RESOLUTION)] = float4(result.rgb, 1.0f);
		}
		// bottom right
		else if (groupThreadID.x == (PROBE_RESOLUTION - 1) && groupThreadID.y == (PROBE_RESOLUTION - 1))
		{
			g_RWTextures[g_PushConsts.resultTextureIndex][outputCoord + int2(-PROBE_RESOLUTION, -PROBE_RESOLUTION)] = float4(result.rgb, 1.0f);
		}
	}
}