#include "bindings.hlsli"

struct Constants
{
	float4x4 prevViewMatrix;
	float4x4 prevProjMatrix;
	float4 volumeResResultRes;
	float4 viewMatrixDepthRow;
	float3 frustumCornerTL;
	float volumeDepth;
	float3 frustumCornerTR;
	float volumeNear;
	float3 frustumCornerBL;
	float volumeFar;
	float3 frustumCornerBR;
	float jitterX;
	float3 cameraPos;
	float jitterY;
	float2 volumeTexelSize;
	float jitterZ;
	uint directionalLightCount;
	uint directionalLightBufferIndex;
	uint directionalLightShadowedCount;
	uint directionalLightShadowedBufferIndex;
	uint punctualLightCount;
	uint punctualLightBufferIndex;
	uint punctualLightTileTextureIndex;
	uint punctualLightDepthBinsBufferIndex;
	uint punctualLightShadowedCount;
	uint punctualLightShadowedBufferIndex;
	uint punctualLightShadowedTileTextureIndex;
	uint punctualLightShadowedDepthBinsBufferIndex;
	uint globalMediaCount;
	uint globalMediaBufferIndex;
	uint localMediaCount;
	uint localMediaBufferIndex;
	uint localMediaTileTextureIndex;
	uint localMediaDepthBinsBufferIndex;
	uint exposureBufferIndex;
	uint scatterResultTextureIndex;
	uint filterInputTextureIndex;
	uint filterHistoryTextureIndex;
	uint filterResultTextureIndex;
	uint integrateInputTextureIndex;
	uint integrateResultTextureIndex;
	float filterAlpha;
	uint checkerBoardCondition;
	uint ignoreHistory;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);

Texture3D<float4> g_Textures3D[65536] : REGISTER_SRV(0, 0, 1);
ByteAddressBuffer g_ByteAddressBuffers[65536] : REGISTER_SRV(4, 1, 1);
RWTexture3D<float4> g_RWTextures3D[65536] : REGISTER_UAV(1, 2, 1);

SamplerState g_LinearClampSampler : REGISTER_SAMPLER(0, 0, 2);

float3 calcWorldSpacePos(float3 texelCoord)
{
	float2 uv = texelCoord.xy * g_Constants.volumeTexelSize;
	
	float3 pos = lerp(g_Constants.frustumCornerTL, g_Constants.frustumCornerTR, uv.x);
	pos = lerp(pos, lerp(g_Constants.frustumCornerBL, g_Constants.frustumCornerBR, uv.x), uv.y);
	
	float d = texelCoord.z * rcp(g_Constants.volumeDepth);
	float z = g_Constants.volumeNear * exp2(d * (log2(g_Constants.volumeFar / g_Constants.volumeNear)));
	pos *= z / g_Constants.volumeFar;
	
	pos += g_Constants.cameraPos;
	
	return pos;
}

[numthreads(4, 4, 4)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float4 result = 0.0;
	bool checkerboardHole = (((threadID.x + threadID.y) & 1) == g_Constants.checkerBoardCondition);
	checkerboardHole = (threadID.z & 1) ? !checkerboardHole : checkerboardHole;
	if (!checkerboardHole)
	{
		result = g_Textures3D[g_Constants.filterInputTextureIndex].Load(uint4(threadID.xy, threadID.z / 2, 0));
	}
	
	// reproject and combine with previous result from previous frame
	if (g_Constants.ignoreHistory == 0)
	{
		float4 prevViewSpacePos = mul(g_Constants.prevViewMatrix, float4(calcWorldSpacePos(threadID + 0.5f), 1.0f));
		
		float z = -prevViewSpacePos.z;//length(prevViewSpacePos.xyz);
		float d = log2(max(0, z * rcp(g_Constants.volumeNear))) * rcp(log2(g_Constants.volumeFar / g_Constants.volumeNear));

		float4 prevClipSpacePos = mul(g_Constants.prevProjMatrix, prevViewSpacePos);
		float3 prevTexCoord = float3((prevClipSpacePos.xy / prevClipSpacePos.w) * float2(0.5f, -0.5f) + 0.5f, d);
		
		bool validCoord = all(prevTexCoord >= 0.0 && prevTexCoord <= 1.0);
		float4 prevResult = 0.0;
		if (validCoord)
		{
			prevResult = g_Textures3D[g_Constants.filterHistoryTextureIndex].SampleLevel(g_LinearClampSampler, prevTexCoord, 0.0);
			
			// prevResult.rgb is pre-exposed -> convert from previous frame exposure to current frame exposure
			prevResult.rgb *= asfloat(g_ByteAddressBuffers[g_Constants.exposureBufferIndex].Load(4)); // 0 = current frame exposure | 4 = previous frame to current frame exposure
			
			result = checkerboardHole ? prevResult : lerp(prevResult, result, g_Constants.filterAlpha);
		}
		
		if (!validCoord && checkerboardHole)
		{
			result = 0.0f;
			result += g_Textures3D[g_Constants.filterInputTextureIndex].Load(uint4((int2)threadID.xy + int2(0, 1), threadID.z / 2, 0));
			result += g_Textures3D[g_Constants.filterInputTextureIndex].Load(uint4((int2)threadID.xy + int2(-1, 0), threadID.z / 2, 0));
			result += g_Textures3D[g_Constants.filterInputTextureIndex].Load(uint4((int2)threadID.xy + int2(1, 0), threadID.z / 2, 0));
			result += g_Textures3D[g_Constants.filterInputTextureIndex].Load(uint4((int2)threadID.xy + int2(0, -1), threadID.z / 2, 0));
			result *= 0.25f;
		}
	}
	
	g_RWTextures3D[g_Constants.filterResultTextureIndex][threadID] = result;
}