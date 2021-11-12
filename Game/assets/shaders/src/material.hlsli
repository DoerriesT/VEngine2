#ifndef MATERIAL_H
#define MATERIAL_H

struct Material
{
	float3 emissive;
	uint alphaMode;
	uint albedo;
	float metalness;
	float roughness;
	uint albedoTextureHandle;
	uint normalTextureHandle;
	uint metalnessTextureHandle;
	uint roughnessTextureHandle;
	uint occlusionTextureHandle;
	uint emissiveTextureHandle;
	uint displacementTextureHandle;
	uint pad0;
	uint pad1;
};

#endif // MATERIAL_H