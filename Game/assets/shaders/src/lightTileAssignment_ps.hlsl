#include "bindings.hlsli"

#define LIGHTING_TILE_SIZE 8

struct PSInput
{
	float4 position : SV_Position;
#if VULKAN
	[[vk::builtin("HelperInvocation")]] bool helperInvocation : HELPER_INVOCATION;
#endif // VULKAN
};

struct PassConstants
{
	uint tileCountX;
	uint transformBufferIndex;
	uint wordCount;
	uint resultTextureIndex;
};

struct PushConsts
{
	uint lightIndex;
	uint transformIndex;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);

RWTexture2DArray<uint> g_RWArrayTextures[65536] : REGISTER_UAV(1, 1, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

uint WaveCompactValue(uint checkValue, bool helperInvocation)
{
	uint index = 128; // index of all threads with the same checkValue
	for (;;) // loop until all active threads are removed
	{
		if (helperInvocation)
		{
			break;
		}
		uint firstValue = WaveReadLaneFirst(checkValue);

		// exclude all threads with firstValue from next iteration
		if (firstValue == checkValue)
		{
			// get index of this thread in all other threads with the same checkValue
			index = WavePrefixCountBits(firstValue == checkValue);
			break;
		}
	}
	
	return index;
}

void main(PSInput input)
{
	uint2 tile = (uint2(input.position.xy) * 2) / LIGHTING_TILE_SIZE;
	uint tileIndex = tile.x + tile.y * g_PassConstants.tileCountX;
	
	const uint lightBit = 1u << (g_PushConsts.lightIndex % 32u);
	const uint word = g_PushConsts.lightIndex / 32u;
	uint wordIndex = tileIndex * g_PassConstants.wordCount + word;
	
#if VULKAN
	const uint idx = WaveCompactValue(wordIndex, input.helperInvocation);
#else
	const uint idx = WaveCompactValue(wordIndex, false);
#endif // VULKAN
	
	// branch only for first occurrence of unique key within subgroup
	if (idx == 0)
	{
		InterlockedOr(g_RWArrayTextures[g_PassConstants.resultTextureIndex][uint3(tile, word)], lightBit);
	}
}