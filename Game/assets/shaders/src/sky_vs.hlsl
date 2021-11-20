#include "bindings.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float4 ray : RAY;
};

struct PushConsts
{
	float4x4 invModelViewProjection;
	uint exposureBufferIndex;
};

PUSH_CONSTS(PushConsts, g_PushConsts);


VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	float y = -1.0f + float((vertexID & 1) << 2);
	float x = -1.0f + float((vertexID & 2) << 1);
	// set depth to 0 because we use an inverted depth buffer
    output.position = float4(x, y, 0.0f, 1.0f);
	output.ray = mul(g_PushConsts.invModelViewProjection, output.position);
	
	return output;
}