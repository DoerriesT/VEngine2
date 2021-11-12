#include "bindings.hlsli"

struct GSInput
{
	float4 position : SV_Position;
	float3 normal : NORMAL;
	float3 worldSpacePosition : WORLD_SPACE_POS;
};

struct GSOutput
{
	float4 position : SV_Position;
};

struct PassConstants
{
	float4x4 viewProjectionMatrix;
	uint skinningMatricesBufferIndex;
	float normalsLength;
};

ConstantBuffer<PassConstants> g_PassConstants : REGISTER_CBV(0, 0, 0);

[maxvertexcount(6)]
void main(triangle GSInput input[3], inout LineStream<GSOutput> outputStream)
{   
    for(uint i = 0; i < 3; ++i)
    {
		GSOutput output = (GSOutput)0;
		
		output.position = mul(g_PassConstants.viewProjectionMatrix, float4(input[i].worldSpacePosition, 1.0f));
		outputStream.Append(output);
		
		output.position = mul(g_PassConstants.viewProjectionMatrix, float4(input[i].worldSpacePosition + input[i].normal * g_PassConstants.normalsLength, 1.0f));
		outputStream.Append(output);
		
		outputStream.RestartStrip();
    }
}