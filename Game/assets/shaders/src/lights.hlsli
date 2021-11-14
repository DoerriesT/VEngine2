#ifndef LIGHTS_H
#define LIGHTS_H

struct DirectionalLight
{
	float3 color;
	uint shadowTextureHandle;
	float3 direction;
	uint cascadeCount;
	float4x4 shadowMatrices[6];
};

struct PunctualLight
{
	float3 color;
	float invSqrAttRadius;
	float3 position;
	float angleScale;
	float3 direction;
	float angleOffset;
};

struct PunctualLightShadowed
{
	PunctualLight light;
	float4 shadowMatrix0;
	float4 shadowMatrix1;
	float4 shadowMatrix2;
	float4 shadowMatrix3;
	float radius;
	uint shadowTextureHandle;
	float pad1;
	float pad2;
};

#endif // LIGHTS_H