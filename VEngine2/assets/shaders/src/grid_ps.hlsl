#include "bindings.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float3 worldSpacePos : WORLD_SPACE_POS;
	float2 gridPos : GRID_POSITION;
};

struct PSOutput
{
	float4 color : SV_Target0;
};

struct Constants
{
	float4x4 modelMatrix;
	float4x4 viewProjectionMatrix;
	float4 thinLineColor;
	float4 thickLineColor;
	float3 cameraPos;
	float cellSize;
	float3 gridNormal;
	float gridSize;
};

ConstantBuffer<Constants> g_Constants : REGISTER_CBV(0, 0, 0);

float4 main(PSInput input) : SV_Target0
{
	float2 uv = input.gridPos;
	
	// find screen space derivatives of grid space
	float2 dudv = float2(length(float2(ddx(uv.x), ddy(uv.x))), length(float2(ddx(uv.y), ddy(uv.y))));
	
	const float minPixelsBetweenCells = 1.0f;
	
	float lodLevel = max(0.0f, log10((length(dudv) * minPixelsBetweenCells) / g_Constants.cellSize) + 1.0f);
	float lodFade = frac(lodLevel);
	
	
	float lod0CellSize = g_Constants.cellSize * pow(10.0f, floor(lodLevel));
	float lod1CellSize = lod0CellSize * 10.0f;
	float lod2CellSize = lod1CellSize * 10.0f;
	
	// allow each anti-aliased line to cover up to 2 pixels
	dudv *= 2.0f;
	
	float2 lod0CrossA = 1.0f - abs(saturate(fmod(uv, lod0CellSize) / dudv) * 2.0f - 1.0f);
	// pick max of x,y to get a coverage alpha value for lod
	float lod0A = max(lod0CrossA.x, lod0CrossA.y);
	float2 lod1CrossA = 1.0f - abs(saturate(fmod(uv, lod1CellSize) / dudv) * 2.0f - 1.0f);
	float lod1A = max(lod1CrossA.x, lod1CrossA.y);
	float2 lod2CrossA = 1.0f - abs(saturate(fmod(uv, lod2CellSize) / dudv) * 2.0f - 1.0f);
	float lod2A = max(lod2CrossA.x, lod2CrossA.y);
	
	// blend between falloff colors to handle LOD transition
	float4 c = lod2A > 0.0f ? g_Constants.thickLineColor : lod1A > 0.0f ? lerp(g_Constants.thickLineColor, g_Constants.thinLineColor, lodFade) : g_Constants.thinLineColor;
	
	// calculate opacity falloff based on distance to grid extents and gracing angle
	float3 viewDir = normalize(g_Constants.cameraPos - input.worldSpacePos);
	float opacityGracing = 1.0f - pow(1.0f - abs(dot(viewDir, g_Constants.gridNormal)), 16.0f);
	float opacityDistance = (1.0f - saturate(length(uv) / g_Constants.gridSize));
	float opacity = opacityGracing * opacityDistance;
	
	// blend between LOD level alphas and scale with opacity falloff
	c.a *= (lod2A > 0.0f ? lod2A : lod1A > 0.0f ? lod1A : (lod0A * (1.0f - lodFade))) * opacity;
	
	return c;
}