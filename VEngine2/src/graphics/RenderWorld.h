#pragma once
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <stdint.h>
#include "component/LightComponent.h"
#include "component/TransformComponent.h"
#include "Handles.h"
#include "ecs/ECSCommon.h"
#include <EASTL/vector.h>

class ECS;

struct RenderWorld
{
	struct DirectionalLight
	{
		glm::vec3 m_direction;
		uint32_t m_color;
		float m_intensity;
		uint32_t m_cascadeCount;
		float m_splitLambda;
		float m_maxShadowDistance;
		float m_depthBias[LightComponent::k_maxCascades] ;
		float m_normalOffsetBias[LightComponent::k_maxCascades];
		bool m_shadowsEnabled;
	};

	struct PunctualLight
	{
		glm::vec3 m_position;
		glm::vec3 m_direction;
		uint32_t m_color;
		float m_intensity;
		float m_radius;
		float m_outerAngle;
		float m_innerAngle;
		bool m_shadowsEnabled;
		bool m_spotLight;
	};

	struct GlobalParticipatingMedium
	{
		uint32_t m_albedo;
		float m_extinction;
		uint32_t m_emissiveColor;
		float m_emissiveIntensity;
		float m_phaseAnisotropy;
		bool m_heightFogEnabled;
		float m_heightFogStart;
		float m_heightFogFalloff;
		float m_maxHeight;
		TextureHandle m_densityTextureHandle;
		float m_textureScale;
		float m_textureBias[3];
	};

	struct Mesh
	{
		SubMeshHandle m_subMeshHandle;
		MaterialHandle m_materialHandle;
		EntityID m_entity;
		size_t m_transformIndex;
		size_t m_skinningMatricesOffset;
		size_t m_skinningMatricesCount;
		bool m_outlined;
	};

	struct Camera
	{
		Transform m_transform;
		float m_aspectRatio;
		float m_fovy;
		float m_near;
		float m_far;
	};

	size_t m_cameraIndex;
	eastl::vector<Camera> m_cameras;
	eastl::vector<DirectionalLight> m_directionalLights;
	eastl::vector<DirectionalLight> m_directionalLightsShadowed;
	eastl::vector<PunctualLight> m_punctualLights;
	eastl::vector<PunctualLight> m_punctualLightsShadowed;
	eastl::vector<GlobalParticipatingMedium> m_globalParticipatingMedia;
	eastl::vector<Mesh> m_meshes;

	eastl::vector<Transform> m_meshTransforms;
	eastl::vector<Transform> m_prevMeshTransforms;
	eastl::vector<glm::mat4> m_skinningMatrices;
	eastl::vector<glm::mat4> m_prevSkinningMatrices;

	void populate(ECS *ecs, EntityID cameraEntity, float fractionalSimFrameTime) noexcept;
};