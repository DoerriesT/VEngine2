#include "RenderWorld.h"
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/LightComponent.h"
#include "component/ParticipatingMediumComponent.h"
#include "component/CameraComponent.h"
#include "component/OutlineComponent.h"
#include <glm/gtx/transform.hpp>
#include "Log.h"

static Transform lerp(const Transform &x, const Transform &y, float alpha) noexcept
{
	Transform result;
	result.m_translation = glm::mix(x.m_translation, y.m_translation, alpha);
	result.m_rotation = glm::normalize(glm::slerp(x.m_rotation, y.m_rotation, alpha));
	result.m_scale = glm::mix(x.m_scale, y.m_scale, alpha);
	return result;
}

void lerp(size_t count, const glm::mat4 *x, const glm::mat4 *y, float alpha, glm::mat4 *result) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		for (size_t j = 0; j < 16; ++j)
		{
			(&result[i][0][0])[j] = glm::mix((&x[i][0][0])[j], (&y[i][0][0])[j], alpha);
		}
	}
}

void RenderWorld::populate(ECS *ecs, EntityID cameraEntity, float fractionalSimFrameTime) noexcept
{
	m_cameras.clear();
	m_directionalLights.clear();
	m_directionalLightsShadowed.clear();
	m_punctualLights.clear();
	m_punctualLightsShadowed.clear();
	m_globalParticipatingMedia.clear();
	m_meshes.clear();
	m_meshTransforms.clear();
	m_prevMeshTransforms.clear();
	m_skinningMatrices.clear();
	m_prevSkinningMatrices.clear();

	// cameras
	{
		m_cameraIndex = -1;
		ecs->iterate<TransformComponent, CameraComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, CameraComponent *cameraC)
			{
				for (size_t i = 0; i < count; ++i)
				{
					if (entities[i] == cameraEntity)
					{
						m_cameraIndex = m_cameras.size();
					}

					auto &tc = transC[i];
					const auto &cc = cameraC[i];

					Camera camera{};
					camera.m_transform = lerp(tc.m_prevTransform, tc.m_transform, fractionalSimFrameTime);
					camera.m_aspectRatio = cc.m_aspectRatio;
					camera.m_fovy = cc.m_fovy;
					camera.m_near = cc.m_near;
					camera.m_far = cc.m_far;

					tc.m_lastRenderTransform = camera.m_transform;

					m_cameras.push_back(camera);
				}
			});
	}	

	// lights
	ecs->iterate<TransformComponent, LightComponent>([&](size_t count, const EntityID *entities, TransformComponent *transC, LightComponent *lightC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				auto &lc = lightC[i];

				auto transform = lerp(tc.m_prevTransform, tc.m_transform, fractionalSimFrameTime);
				tc.m_lastRenderTransform = transform;

				switch (lc.m_type)
				{
				case LightComponent::Type::Point:
				case LightComponent::Type::Spot:
				{
					PunctualLight punctualLight{};
					punctualLight.m_position = transform.m_translation;
					punctualLight.m_direction = glm::normalize(transform.m_rotation * glm::vec3(0.0f, -1.0f, 0.0f));
					punctualLight.m_color = lc.m_color;
					punctualLight.m_intensity = lc.m_intensity;
					punctualLight.m_radius = lc.m_radius;
					punctualLight.m_outerAngle = lc.m_outerAngle;
					punctualLight.m_innerAngle = lc.m_innerAngle;
					punctualLight.m_shadowsEnabled = lc.m_shadows;
					punctualLight.m_spotLight = lc.m_type == LightComponent::Type::Spot;

					if (lc.m_shadows)
					{
						m_punctualLightsShadowed.push_back(punctualLight);
					}
					else
					{
						m_punctualLights.push_back(punctualLight);
					}
					break;
				}
				case LightComponent::Type::Directional:
				{
					DirectionalLight directionalLight{};
					directionalLight.m_direction = glm::normalize(transform.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
					directionalLight.m_color = lc.m_color;
					directionalLight.m_intensity = lc.m_intensity;
					directionalLight.m_cascadeCount = lc.m_cascadeCount;
					directionalLight.m_splitLambda = lc.m_splitLambda;
					directionalLight.m_maxShadowDistance = lc.m_maxShadowDistance;
					static_assert(sizeof(directionalLight.m_depthBias) == sizeof(lc.m_depthBias));
					memcpy(directionalLight.m_depthBias, lc.m_depthBias, sizeof(directionalLight.m_depthBias));
					static_assert(sizeof(directionalLight.m_normalOffsetBias) == sizeof(lc.m_normalOffsetBias));
					memcpy(directionalLight.m_normalOffsetBias, lc.m_normalOffsetBias, sizeof(directionalLight.m_normalOffsetBias));
					directionalLight.m_shadowsEnabled = lc.m_shadows;

					if (lc.m_shadows)
					{
						m_directionalLightsShadowed.push_back(directionalLight);
					}
					else
					{
						m_directionalLights.push_back(directionalLight);
					}
					break;
				}
				default:
					assert(false);
					break;
				}
			}
		});

	// participating media
	ecs->iterate<ParticipatingMediumComponent>([&](size_t count, const EntityID *entities, ParticipatingMediumComponent *mediaC)
		{
			for (size_t i = 0; i < count; ++i)
			{
				auto &mc = mediaC[i];

				if (mc.m_type == ParticipatingMediumComponent::Type::Global)
				{
					GlobalParticipatingMedium medium{};
					medium.m_albedo = mc.m_albedo;
					medium.m_extinction = mc.m_extinction;
					medium.m_emissiveColor = mc.m_emissiveColor;
					medium.m_emissiveIntensity = mc.m_emissiveIntensity;
					medium.m_phaseAnisotropy = mc.m_phaseAnisotropy;
					medium.m_heightFogEnabled = mc.m_heightFogEnabled;
					medium.m_heightFogStart = mc.m_heightFogStart;
					medium.m_heightFogFalloff = mc.m_heightFogFalloff;
					medium.m_maxHeight = mc.m_maxHeight;
					medium.m_densityTextureHandle = mc.m_densityTexture.isLoaded() ? mc.m_densityTexture->getTextureHandle() : TextureHandle{};
					medium.m_textureScale = mc.m_textureScale;
					memcpy(medium.m_textureBias, mc.m_textureBias, sizeof(medium.m_textureBias));

					m_globalParticipatingMedia.push_back(medium);
				}
			}
		});

	// meshes
	IterateQuery iterateQuery;
	ecs->setIterateQueryRequiredComponents<TransformComponent>(iterateQuery);
	ecs->setIterateQueryOptionalComponents<MeshComponent, SkinnedMeshComponent, OutlineComponent, EditorOutlineComponent>(iterateQuery);
	ecs->iterate<TransformComponent, MeshComponent, SkinnedMeshComponent, OutlineComponent, EditorOutlineComponent>(
		iterateQuery,
		[&](size_t count, const EntityID *entities, TransformComponent *transC, MeshComponent *meshC, SkinnedMeshComponent *sMeshC, OutlineComponent *outlineC, EditorOutlineComponent *editorOutlineC)
		{
			// we need either one of these
			if (!meshC && !sMeshC)
			{
				return;
			}
			const bool skinned = sMeshC != nullptr;
			for (size_t i = 0; i < count; ++i)
			{
				auto &tc = transC[i];
				const auto &meshAsset = skinned ? sMeshC[i].m_mesh : meshC[i].m_mesh;
				if (!meshAsset.get())
				{
					continue;
				}

				// model transform
				const auto transformIndex = m_meshTransforms.size();
				m_prevMeshTransforms.push_back(tc.m_lastRenderTransform);
				m_meshTransforms.push_back(lerp(tc.m_prevTransform, tc.m_transform, fractionalSimFrameTime));
				tc.m_lastRenderTransform = m_meshTransforms.back();
				
				// skinning matrices
				const auto skinningMatricesOffset = m_skinningMatrices.size();
				if (skinned)
				{
					const auto &palette = sMeshC[i].m_matrixPalette;
					const auto &prevPalette = sMeshC[i].m_prevMatrixPalette;
					auto &lastRenderPalette = sMeshC[i].m_lastRenderMatrixPalette;
					assert(palette.size() == sMeshC[i].m_skeleton->getSkeleton()->getJointCount());
					assert(palette.size() == prevPalette.size());
					assert(palette.size() == lastRenderPalette.size());
					m_skinningMatrices.resize(m_skinningMatrices.size() + palette.size());
					m_prevSkinningMatrices.reserve(m_prevSkinningMatrices.size() + palette.size());
					lerp(palette.size(), prevPalette.data(), palette.data(), fractionalSimFrameTime, m_skinningMatrices.end() - palette.size());
					m_prevSkinningMatrices.insert(m_prevSkinningMatrices.end(), lastRenderPalette.begin(), lastRenderPalette.end());
					lastRenderPalette.clear();
					lastRenderPalette.insert(lastRenderPalette.begin(), m_skinningMatrices.end() - palette.size(), m_skinningMatrices.end());
				}

				const auto &submeshhandles = meshAsset->getSubMeshhandles();
				const auto &materialAssets = meshAsset->getMaterials();
				for (size_t j = 0; j < submeshhandles.size(); ++j)
				{
					Mesh mesh{};
					mesh.m_subMeshHandle = submeshhandles[j];
					mesh.m_materialHandle = materialAssets[j]->getMaterialHandle();
					mesh.m_entity = entities[i];
					mesh.m_transformIndex = transformIndex;
					mesh.m_skinningMatricesOffset = skinned ? skinningMatricesOffset : 0;
					mesh.m_skinningMatricesCount = skinned ? sMeshC[i].m_matrixPalette.size() : 0;
					mesh.m_outlined = (outlineC && outlineC[i].m_outlined) || (editorOutlineC && editorOutlineC[i].m_outlined);

					m_meshes.push_back(mesh);
				}
			}
		});
}
