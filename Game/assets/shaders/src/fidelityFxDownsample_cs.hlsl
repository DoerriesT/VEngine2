#include "bindings.hlsli"

enum ReductionType
{
	REDUCTION_TYPE_AVG = 0,
	REDUCTION_TYPE_MIN = 1,
	REDUCTION_TYPE_MAX = 2,
};

struct FFXDownsampleConstants
{
	uint mips;
	uint numWorkGroups;
	uint2 workGroupOffset;
	float2 inputTexelSize;
	ReductionType reductionType;
	uint arrayTextureInput;
	uint inputTextureIndex;
	uint resultMip0TextureIndex;
	uint resultMip1TextureIndex;
	uint resultMip2TextureIndex;
	uint resultMip3TextureIndex;
	uint resultMip4TextureIndex;
	uint resultMip5TextureIndex;
	uint resultMip6TextureIndex;
	uint resultMip7TextureIndex;
	uint resultMip8TextureIndex;
	uint resultMip9TextureIndex;
	uint resultMip10TextureIndex;
	uint resultMip11TextureIndex;
	uint spdCounterBufferIndex;
};

ConstantBuffer<FFXDownsampleConstants> g_Constants : REGISTER_CBV(0, 0, 0);

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 1);
RWTexture2D<float4> g_RWTextures[65536] : REGISTER_UAV(1, 1, 1);
globallycoherent RWTexture2D<float4> g_RWTexturesCoherent[65536] : REGISTER_UAV(1, 2, 1);

Texture2DArray<float4> g_ArrayTextures[65536] : REGISTER_SRV(0, 3, 1);
RWTexture2DArray<float4> g_RWArrayTextures[65536] : REGISTER_UAV(1, 4, 1);
globallycoherent RWTexture2DArray<float4> g_RWArrayTexturesCoherent[65536] : REGISTER_UAV(1, 5, 1);

globallycoherent RWByteAddressBuffer g_RWByteAddressBuffers[65536] : REGISTER_UAV(5, 6, 1);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

#define A_GPU 1
#define A_HLSL 1
#include "../../../../libs/include/ffx_a.h"

groupshared AU1 spdCounter;
groupshared AF1 spdIntermediateR[16][16];
groupshared AF1 spdIntermediateG[16][16];
groupshared AF1 spdIntermediateB[16][16];
groupshared AF1 spdIntermediateA[16][16];

AF4 SpdLoadSourceImage(ASU2 coord, AU1 slice)
{
	float2 uv = coord * g_Constants.inputTexelSize + g_Constants.inputTexelSize;
	
	if (g_Constants.reductionType == REDUCTION_TYPE_AVG)
	{
		if (!g_Constants.arrayTextureInput)
		{
			return g_Textures[g_Constants.inputTextureIndex].SampleLevel(g_LinearClampSampler, uv, 0.0f);
		}
		else
		{
			return g_ArrayTextures[g_Constants.inputTextureIndex].SampleLevel(g_LinearClampSampler, float3(uv, slice), 0.0f);
		}
	}
	else
	{
		float4 red4, green4, blue4, alpha4;
		if (!g_Constants.arrayTextureInput)
		{
			red4 = g_Textures[g_Constants.inputTextureIndex].GatherRed(g_LinearClampSampler, uv);
			green4 = g_Textures[g_Constants.inputTextureIndex].GatherGreen(g_LinearClampSampler, uv);
			blue4 = g_Textures[g_Constants.inputTextureIndex].GatherBlue(g_LinearClampSampler, uv);
			alpha4 = g_Textures[g_Constants.inputTextureIndex].GatherAlpha(g_LinearClampSampler, uv);
		}
		else
		{
			red4 = g_ArrayTextures[g_Constants.inputTextureIndex].GatherRed(g_LinearClampSampler, float3(uv, slice));
			green4 = g_ArrayTextures[g_Constants.inputTextureIndex].GatherGreen(g_LinearClampSampler, float3(uv, slice));
			blue4 = g_ArrayTextures[g_Constants.inputTextureIndex].GatherBlue(g_LinearClampSampler, float3(uv, slice));
			alpha4 = g_ArrayTextures[g_Constants.inputTextureIndex].GatherAlpha(g_LinearClampSampler, float3(uv, slice));
		}
		
		float4 col0 = float4(red4.x, blue4.x, green4.x, alpha4.x);
		float4 col1 = float4(red4.y, blue4.y, green4.y, alpha4.y);
		float4 col2 = float4(red4.z, blue4.z, green4.z, alpha4.z);
		float4 col3 = float4(red4.w, blue4.w, green4.w, alpha4.w);

		if (g_Constants.reductionType == REDUCTION_TYPE_MIN)
		{
			return min(min(col0, col1), min(col2, col3));
		}
		else
		{
			return max(max(col0, col1), max(col2, col3));
		}
	}
}

AF4 SpdLoad(ASU2 coord, AU1 slice)
{
	if (!g_Constants.arrayTextureInput)
	{
		return g_RWTexturesCoherent[g_Constants.resultMip5TextureIndex].Load(coord);
	}
	else
	{
		return g_RWArrayTexturesCoherent[g_Constants.resultMip5TextureIndex].Load(uint3(coord, slice));
	}
}

void SpdStore(ASU2 coord, AF4 outValue, AU1 index, AU1 slice)
{
	if (!g_Constants.arrayTextureInput)
	{
		switch (index)
		{
			case 0: g_RWTextures[g_Constants.resultMip0TextureIndex][coord] = outValue; break;
			case 1: g_RWTextures[g_Constants.resultMip1TextureIndex][coord] = outValue; break;
			case 2: g_RWTextures[g_Constants.resultMip2TextureIndex][coord] = outValue; break;
			case 3: g_RWTextures[g_Constants.resultMip3TextureIndex][coord] = outValue; break;
			case 4: g_RWTextures[g_Constants.resultMip4TextureIndex][coord] = outValue; break;
			case 5: g_RWTexturesCoherent[g_Constants.resultMip5TextureIndex][coord] = outValue; break; // coherent
			case 6: g_RWTextures[g_Constants.resultMip6TextureIndex][coord] = outValue; break;
			case 7: g_RWTextures[g_Constants.resultMip7TextureIndex][coord] = outValue; break;
			case 8: g_RWTextures[g_Constants.resultMip8TextureIndex][coord] = outValue; break;
			case 9: g_RWTextures[g_Constants.resultMip9TextureIndex][coord] = outValue; break;
			case 10: g_RWTextures[g_Constants.resultMip10TextureIndex][coord] = outValue; break;
			case 11: g_RWTextures[g_Constants.resultMip11TextureIndex][coord] = outValue; break;
		}
	}
	else
	{
		switch (index)
		{
			case 0: g_RWArrayTextures[g_Constants.resultMip0TextureIndex][uint3(coord, slice)] = outValue; break;
			case 1: g_RWArrayTextures[g_Constants.resultMip1TextureIndex][uint3(coord, slice)] = outValue; break;
			case 2: g_RWArrayTextures[g_Constants.resultMip2TextureIndex][uint3(coord, slice)] = outValue; break;
			case 3: g_RWArrayTextures[g_Constants.resultMip3TextureIndex][uint3(coord, slice)] = outValue; break;
			case 4: g_RWArrayTextures[g_Constants.resultMip4TextureIndex][uint3(coord, slice)] = outValue; break;
			case 5: g_RWArrayTexturesCoherent[g_Constants.resultMip5TextureIndex][uint3(coord, slice)] = outValue; break; // coherent
			case 6: g_RWArrayTextures[g_Constants.resultMip6TextureIndex][uint3(coord, slice)] = outValue; break;
			case 7: g_RWArrayTextures[g_Constants.resultMip7TextureIndex][uint3(coord, slice)] = outValue; break;
			case 8: g_RWArrayTextures[g_Constants.resultMip8TextureIndex][uint3(coord, slice)] = outValue; break;
			case 9: g_RWArrayTextures[g_Constants.resultMip9TextureIndex][uint3(coord, slice)] = outValue; break;
			case 10: g_RWArrayTextures[g_Constants.resultMip10TextureIndex][uint3(coord, slice)] = outValue; break;
			case 11: g_RWArrayTextures[g_Constants.resultMip11TextureIndex][uint3(coord, slice)] = outValue; break;
		}
	}
}

void SpdIncreaseAtomicCounter(AU1 slice)
{
	g_RWByteAddressBuffers[g_Constants.spdCounterBufferIndex].InterlockedAdd(slice << 2, 1, spdCounter);
}

AU1 SpdGetAtomicCounter()
{
    return spdCounter;
}

void SpdResetAtomicCounter(AU1 slice)
{
	g_RWByteAddressBuffers[g_Constants.spdCounterBufferIndex].Store(slice << 2, 0);
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y)
{
    return AF4(
		spdIntermediateR[x][y],
		spdIntermediateG[x][y],
		spdIntermediateB[x][y],
		spdIntermediateA[x][y]
	);
}

void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
	switch (g_Constants.reductionType)
	{
		case REDUCTION_TYPE_AVG: return (v0 + v1 + v2 + v3) * 0.25f;
		case REDUCTION_TYPE_MIN: return min(min(v0, v1), min(v2, v3));
		case REDUCTION_TYPE_MAX: return max(max(v0, v1), max(v2, v3));
	}
	return 0.0f;
}

#define SPD_LINEAR_SAMPLER
#include "../../../../libs/include/ffx_spd.h"


[numthreads(256, 1, 1)]
void main(uint3 WorkGroupId : SV_GroupID, uint LocalThreadIndex : SV_GroupIndex)
{
	SpdDownsample(
        AU2(WorkGroupId.xy), 
        AU1(LocalThreadIndex),  
        AU1(g_Constants.mips),
        AU1(g_Constants.numWorkGroups),
        AU1(WorkGroupId.z),
        AU2(g_Constants.workGroupOffset));
}