#pragma once
#include "Handles.h"
#include "ViewHandles.h"

struct MaterialCreateInfo
{
	enum class Alpha : uint32_t
	{
		OPAQUE, MASKED, BLENDED
	};

	Alpha m_alpha;
	uint32_t m_albedoFactor;
	float m_metallicFactor;
	float m_roughnessFactor;
	float m_emissiveFactor[3];
	TextureHandle m_albedoTexture;
	TextureHandle m_normalTexture;
	TextureHandle m_metallicTexture;
	TextureHandle m_roughnessTexture;
	TextureHandle m_occlusionTexture;
	TextureHandle m_emissiveTexture;
	TextureHandle m_displacementTexture;
};

struct MaterialGPU
{
	float m_emissive[3];
	MaterialCreateInfo::Alpha m_alphaMode;
	uint32_t m_albedo;
	float m_metalness;
	float m_roughness;
	TextureViewHandle m_albedoTextureHandle;
	TextureViewHandle m_normalTextureHandle;
	TextureViewHandle m_metalnessTextureHandle;
	TextureViewHandle m_roughnessTextureHandle;
	TextureViewHandle m_occlusionTextureHandle;
	TextureViewHandle m_emissiveTextureHandle;
	TextureViewHandle m_displacementTextureHandle;
};