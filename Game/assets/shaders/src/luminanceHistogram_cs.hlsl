#include "bindings.hlsli"

#define LOCAL_SIZE_X 64
#define LUMINANCE_HISTOGRAM_SIZE 256

struct PushConsts
{
	uint width;
	float scale;
	float bias;
	uint inputTextureIndex;
	uint exposureBufferIndex;
	uint resultLuminanceBufferIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
ByteAddressBuffer g_ByteAdressBuffers[65536] : REGISTER_SRV(4, 1, 0);
RWByteAddressBuffer g_RWByteAddressBuffers[65536] : REGISTER_UAV(5, 0, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_localHistogram[LUMINANCE_HISTOGRAM_SIZE];

[numthreads(LOCAL_SIZE_X, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
	// clear local histogram
	{
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			s_localHistogram[i] = 0;
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// each work group computes a local histogram for one horizontal line in the input image
	{
		// input data is pre exposed, so we need to inverse that
		const float invExposure = 1.0f / asfloat(g_ByteAdressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
		const uint width = g_PushConsts.width;
		for (uint i = groupThreadID.x; i < width; i += LOCAL_SIZE_X)
		{
			if (i < width)
			{
				float3 color = g_Textures[g_PushConsts.inputTextureIndex].Load(int3(i, groupID.x, 0)).rgb * invExposure;
				float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
			
				luma = log2(max(luma, 1e-5f)) * g_PushConsts.scale + g_PushConsts.bias;//log(luma + 1.0) * 128;
				
				uint bucket = uint(saturate(luma) * (LUMINANCE_HISTOGRAM_SIZE - 1));//min(uint(luma), 255);
				
				InterlockedAdd(s_localHistogram[bucket], 1);
			}
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	// merge with global histogram
	{
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			g_RWByteAddressBuffers[g_PushConsts.resultLuminanceBufferIndex].InterlockedAdd(i << 2, s_localHistogram[i]);
		}
	}
}