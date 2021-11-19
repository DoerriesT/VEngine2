
struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	float y = -1.0f + float((vertexID & 1) << 2);
	float x = -1.0f + float((vertexID & 2) << 1);

    output.position = float4(x, y, 0.0f, 1.0f);
	output.texCoord = output.position.xy * float2(0.5f, -0.5f) + 0.5f;
	
	return output;
}