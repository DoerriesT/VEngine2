#include "RenderWorld.h"
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/LightComponent.h"
#include "component/CameraComponent.h"
#include "component/OutlineComponent.h"
#include <glm/gtx/transform.hpp>
#include "Log.h"

void RenderWorld::interpolate(float fractionalSimFrameTime) noexcept
{
	// transforms
	{
		const size_t transformCount = m_transforms.size();
		m_interpolatedTransforms.clear();
		m_interpolatedTransforms.resize(transformCount);

		for (size_t i = 0; i < transformCount; ++i)
		{
			m_interpolatedTransforms[i].m_translation = glm::mix(m_prevTransforms[i].m_translation, m_transforms[i].m_translation, fractionalSimFrameTime);
			m_interpolatedTransforms[i].m_rotation = glm::normalize(glm::slerp(m_prevTransforms[i].m_rotation, m_transforms[i].m_rotation, fractionalSimFrameTime));
			m_interpolatedTransforms[i].m_scale = glm::mix(m_prevTransforms[i].m_scale, m_transforms[i].m_scale, fractionalSimFrameTime);
		}
	}

	// lights
	{
		for (auto &directionalLight : m_directionalLights)
		{
			directionalLight.m_direction = glm::normalize(m_interpolatedTransforms[directionalLight.m_transformIndex].m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
		}

		for (auto &punctualLight : m_punctualLights)
		{
			punctualLight.m_position = m_interpolatedTransforms[punctualLight.m_transformIndex].m_translation;
			punctualLight.m_direction = punctualLight.m_spotLight ? 
				glm::normalize(m_interpolatedTransforms[punctualLight.m_transformIndex].m_rotation * glm::vec3(0.0f, -1.0f, 0.0f)) :
				glm::vec3(0.0f, -1.0f, 0.0f);
		}
	}

	// skinning matrices
	{
		const size_t skinningMatrixCount = m_skinningMatrices.size();
		m_interpolatedSkinningMatrices.clear();
		m_interpolatedSkinningMatrices.resize(skinningMatrixCount);

		for (size_t i = 0; i < skinningMatrixCount; ++i)
		{
			for (size_t j = 0; j < 16; ++j)
			{
				(&m_interpolatedSkinningMatrices[i][0][0])[j] = glm::mix((&m_prevSkinningMatrices[i][0][0])[j], (&m_skinningMatrices[i][0][0])[j], fractionalSimFrameTime);
			}
		}
	}
}

void RenderWorld::populate(ECS *ecs, EntityID cameraEntity) noexcept
{
	m_cameras.clear();
	m_directionalLights.clear();
	m_punctualLights.clear();
	m_meshes.clear();
	m_transforms.clear();
	m_prevTransforms.clear();
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

					const auto &tc = transC[i];
					const auto &cc = cameraC[i];

					Camera camera{};
					camera.m_aspectRatio = cc.m_aspectRatio;
					camera.m_fovy = cc.m_fovy;
					camera.m_near = cc.m_near;
					camera.m_far = cc.m_far;
					camera.m_transformIndex = m_transforms.size();

					m_transforms.push_back(tc.m_transform);
					m_prevTransforms.push_back(tc.m_prevTransform);
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

				switch (lc.m_type)
				{
				case LightComponent::Type::Point:
				case LightComponent::Type::Spot:
				{
					PunctualLight punctualLight{};
					//punctualLight.m_position = tc.m_translation;
					//punctualLight.m_direction = glm::normalize(tc.m_rotation * glm::vec3(0.0f, -1.0f, 0.0f));
					punctualLight.m_color = lc.m_color;
					punctualLight.m_intensity = lc.m_intensity;
					punctualLight.m_radius = lc.m_radius;
					punctualLight.m_outerAngle = lc.m_outerAngle;
					punctualLight.m_innerAngle = lc.m_innerAngle;
					punctualLight.m_shadowsEnabled = lc.m_shadows;
					punctualLight.m_spotLight = lc.m_type == LightComponent::Type::Spot;
					punctualLight.m_transformIndex = m_transforms.size();

					m_punctualLights.push_back(punctualLight);
					break;
				}
				case LightComponent::Type::Directional:
				{
					DirectionalLight directionalLight{};
					//directionalLight.m_direction = glm::normalize(tc.m_rotation * glm::vec3(0.0f, 1.0f, 0.0f));
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
					directionalLight.m_transformIndex = m_transforms.size();

					m_directionalLights.push_back(directionalLight);
					break;
				}
				default:
					assert(false);
					break;
				}

				m_transforms.push_back(tc.m_transform);
				m_prevTransforms.push_back(tc.m_prevTransform);
			}
		});

	m_meshTransformsOffset = m_transforms.size();

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
				const auto transformIndex = m_transforms.size();
				m_transforms.push_back(tc.m_transform);
				m_prevTransforms.push_back(tc.m_prevTransform);
				
				// skinning matrices
				const auto skinningMatricesOffset = m_skinningMatrices.size();
				if (skinned)
				{
					const auto &palette = sMeshC[i].m_matrixPalette;
					const auto &prevPalette = sMeshC[i].m_prevMatrixPalette;
					assert(palette.size() == sMeshC[i].m_skeleton->getSkeleton()->getJointCount());
					assert(palette.size() == prevPalette.size());
					m_skinningMatrices.insert(m_skinningMatrices.end(), palette.data(), palette.data() + palette.size());
					m_prevSkinningMatrices.insert(m_prevSkinningMatrices.end(), prevPalette.data(), prevPalette.data() + prevPalette.size());
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

	m_meshTransformsCount = m_transforms.size() - m_meshTransformsOffset;
}
