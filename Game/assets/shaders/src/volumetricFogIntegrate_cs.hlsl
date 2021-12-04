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
RWTexture3D<float4> g_RWTextures3D[65536] : REGISTER_UAV(1, 1, 1);


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

float4 scatterStep(float3 accumulatedLight, float accumulatedTransmittance, float3 sliceLight, float sliceExtinction, float stepLength)
{
	sliceExtinction = max(sliceExtinction, 1e-5);
	float sliceTransmittance = exp(-sliceExtinction * stepLength);
	
	float3 sliceLightIntegral = (-sliceLight * sliceTransmittance + sliceLight) * rcp(sliceExtinction);
	
	accumulatedLight += sliceLightIntegral * accumulatedTransmittance;
	accumulatedTransmittance *= sliceTransmittance;
	
	return float4(accumulatedLight, accumulatedTransmittance);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	float4 accum = float4(0.0f, 0.0f, 0.0f, 1.0f);
	
	float3 prevWorldSpacePos = calcWorldSpacePos(float3(threadID.xy + 0.5f, 0.0f));
	
	for (int z = 0; z < g_Constants.volumeDepth; ++z)
	{
		float3 worldSpacePos = calcWorldSpacePos(float3(threadID.xy + 0.5f, z + 1.0f));
		float stepLength = distance(prevWorldSpacePos, worldSpacePos);
		prevWorldSpacePos = worldSpacePos;
		int4 pos = int4(threadID.xy, z, 0);
		float4 slice = g_Textures3D[g_Constants.integrateInputTextureIndex].Load(pos);
		accum = scatterStep(accum.rgb, accum.a, slice.rgb, slice.a, stepLength);
		float4 result = accum;
		//result.rgb = simpleTonemap(result.rgb);
		g_RWTextures3D[g_Constants.integrateResultTextureIndex][pos.xyz] = result;
	}
}