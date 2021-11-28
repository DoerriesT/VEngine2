
struct PSInput
{
	float4 position : SV_Position;
	float4 color : COLOR;
};

[earlydepthstencil]
float4 main(PSInput input) : SV_Target0
{
	return input.color;
}