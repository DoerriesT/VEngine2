#include "bindings.hlsli"
#include "packing.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
};

struct PushConsts
{
	float4x4 viewProjection;
	float3 worldSpacePosition;
	uint irradianceVolumeTextureIndex;
	uint3 probeIdx;
	uint exposureBufferIndex;
};

Texture2D<float4> g_Textures[65536] : REGISTER_SRV(0, 0, 0);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 1, 0);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

float3 sampleIrradianceProbe(Texture2D<float4> tex, SamplerState linearSampler, uint3 probeIdx, uint3 volumeSize, float3 dir, float2 volumeTexelSize)
{
	const float probeSideLength = 8.0f;
	const float probeSideLengthWithPadding = probeSideLength + 2.0f;
	
	float2 texelCoord = 1.0f;
	texelCoord.x += (volumeSize.x * probeIdx.y + probeIdx.x) * probeSideLengthWithPadding;
	texelCoord.y += probeIdx.z * probeSideLengthWithPadding;
	texelCoord += (encodeOctahedron(dir) * 0.5f + 0.5f) * probeSideLength;
	
	float2 uv = texelCoord * volumeTexelSize;
	
	return tex.SampleLevel(linearSampler, uv, 0.0f).rgb;
}

float4 main(PSInput input) : SV_Target0
{
	const uint3 volumeSize = uint3(15, 6, 8);
	const float probeSideLength = 8.0f;
	const float probeSideLengthWithPadding = probeSideLength + 2.0f;
	
	float3 N = normalize(input.normal);
	//float2 volumeTexelSize;
	//g_Textures[g_PushConsts.irradianceVolumeTextureIndex].GetDimensions(volumeTexelSize.x, volumeTexelSize.y);
	//volumeTexelSize = 1.0f / volumeTexelSize;
	const float2 volumeTexelSize = 1.0f / float2(volumeSize.x * volumeSize.y * (8 + 2), volumeSize.z * (8 + 2));
	
	const float3 irradiance = sampleIrradianceProbe(g_Textures[g_PushConsts.irradianceVolumeTextureIndex], g_LinearClampSampler, g_PushConsts.probeIdx, volumeSize, N, volumeTexelSize);
	const float exposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	
	return float4(irradiance * exposure, 1.0f);
	
	//float2 texelCoord = 1.0f;
	//texelCoord.x += (volumeSize.x * g_PushConsts.probeIdx.y + g_PushConsts.probeIdx.x) * probeSideLengthWithPadding;
	//texelCoord.y += g_PushConsts.probeIdx.z * probeSideLengthWithPadding;
	//texelCoord += (encodeOctahedron(N) * 0.5f + 0.5f) * probeSideLength;
	//
	//float2 res;
	//g_Textures[g_PushConsts.irradianceVolumeTextureIndex].GetDimensions(res.x, res.y);
	//
	//float2 uv = texelCoord / res;
	//
	//const float exposure = asfloat(g_ByteAddressBuffers[g_PushConsts.exposureBufferIndex].Load(0));
	//return float4(g_Textures[g_PushConsts.irradianceVolumeTextureIndex].SampleLevel(g_LinearClampSampler, uv, 0.0f).rgb * exposure, 1.0f);
}