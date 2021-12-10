#pragma once
#include <stdint.h>
#include "Handles.h"
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>
#include "component/LightComponent.h"
#include "ecs/ECSCommon.h"
#include "ViewHandles.h"

struct SubMeshInstanceData
{
	SubMeshHandle m_subMeshHandle;
	uint32_t m_transformIndex;
	uint32_t m_skinningMatricesOffset;
	MaterialHandle m_materialHandle;
	EntityID m_entityID;
};

struct RenderList
{
	eastl::vector<SubMeshInstanceData> m_opaque;
	eastl::vector<SubMeshInstanceData> m_opaqueAlphaTested;
	eastl::vector<SubMeshInstanceData> m_opaqueSkinned;
	eastl::vector<SubMeshInstanceData> m_opaqueSkinnedAlphaTested;

	void clear() noexcept
	{
		m_opaque.clear();
		m_opaqueAlphaTested.clear();
		m_opaqueSkinned.clear();
		m_opaqueSkinnedAlphaTested.clear();
	}
};

struct DirectionalLightGPU
{
	glm::vec3 m_color;
	TextureViewHandle m_shadowTextureHandle;
	glm::vec3 m_direction;
	uint32_t m_cascadeCount;
	glm::mat4 m_shadowMatrices[LightComponent::k_maxCascades];
};

struct PunctualLightGPU
{
	glm::vec3 m_color;
	float m_invSqrAttRadius;
	glm::vec3 m_position;
	float m_angleScale;
	glm::vec3 m_direction;
	float m_angleOffset;
};

struct PunctualLightShadowedGPU
{
	PunctualLightGPU m_light;
	glm::vec4 m_shadowMatrix0; // aliased with first 4 point light shadow map indices
	glm::vec4 m_shadowMatrix1; // aliased with last 2 point light shadow map indices. has depth projection params in zw components.
	glm::vec4 m_shadowMatrix2;
	glm::vec4 m_shadowMatrix3;
	float m_radius;
	uint32_t m_shadowTextureHandle;
	float m_pad1;
	float m_pad2;
};

struct GlobalParticipatingMediumGPU
{
	glm::vec3 emissive;
	float extinction;
	glm::vec3 scattering;
	float phase;
	uint32_t heightFogEnabled;
	float heightFogStart;
	float heightFogFalloff;
	float maxHeight;
	glm::vec3 textureBias;
	float textureScale;
	uint32_t densityTexture;
	float pad0;
	float pad1;
	float pad2;
};

enum class DebugDrawVisibility
{
	Always = 0, Visible = 1, Occluded = 2
};

struct DebugDrawVertex
{
	glm::vec3 m_position;
	uint32_t m_color;
};