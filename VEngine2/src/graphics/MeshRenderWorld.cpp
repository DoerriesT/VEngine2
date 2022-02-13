#include "MeshRenderWorld.h"
#include "FrustumCulling.h"
#include "utility/Utility.h"
#include <assert.h>
#include <EASTL/sort.h>
#include "ecs/ECS.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/OutlineComponent.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "MaterialManager.h"
#include "MeshManager.h"
#include "ResourceViewRegistry.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "LinearGPUBufferAllocator.h"

MeshRenderWorld::MeshRenderWorld(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_meshManager(meshManager),
	m_materialManager(materialManager)
{
}

void MeshRenderWorld::update(ECS *ecs, LinearGPUBufferAllocator *shaderResourceBufferAllocator) noexcept
{
	m_meshInstanceBoundingSpheres.clear();
	m_submeshInstanceBoundingSpheres.clear();
	m_meshInstances.clear();
	m_submeshInstances.clear();
	m_modelMatrices.clear();
	m_prevModelMatrices.clear();
	m_skinningMatrices.clear();
	m_prevSkinningMatrices.clear();

	const SubMeshDrawInfo *subMeshDrawInfoTable = m_meshManager->getSubMeshDrawInfoTable();

	IterateQuery meshIterateQuery;
	ecs->setIterateQueryRequiredComponents<TransformComponent>(meshIterateQuery);
	ecs->setIterateQueryOptionalComponents<MeshComponent, SkinnedMeshComponent, OutlineComponent, EditorOutlineComponent>(meshIterateQuery);
	ecs->iterate<TransformComponent, MeshComponent, SkinnedMeshComponent, OutlineComponent, EditorOutlineComponent>(
		meshIterateQuery,
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
				const auto transformIndex = m_modelMatrices.size();
				m_modelMatrices.push_back(glm::translate(tc.m_curRenderTransform.m_translation) * glm::mat4_cast(tc.m_curRenderTransform.m_rotation) * glm::scale(tc.m_curRenderTransform.m_scale));
				m_prevModelMatrices.push_back(glm::translate(tc.m_prevRenderTransform.m_translation) * glm::mat4_cast(tc.m_prevRenderTransform.m_rotation) * glm::scale(tc.m_prevRenderTransform.m_scale));

				const auto absScale = glm::abs(tc.m_curRenderTransform.m_scale);
				const float boundingSphereScale = fmaxf(absScale.x, fmaxf(absScale.y, absScale.z)) * (skinned ? sMeshC[i].m_boundingSphereSizeFactor : meshC[i].m_boundingSphereSizeFactor);
				const auto bsphereTransform = glm::translate(tc.m_curRenderTransform.m_translation) * glm::mat4_cast(tc.m_curRenderTransform.m_rotation);

				// skinning matrices
				const auto skinningMatricesOffset = m_skinningMatrices.size();
				if (skinned)
				{
					m_skinningMatrices.insert(m_skinningMatrices.end(), sMeshC[i].m_curRenderMatrixPalette.begin(), sMeshC[i].m_curRenderMatrixPalette.end());
					m_prevSkinningMatrices.insert(m_prevSkinningMatrices.end(), sMeshC[i].m_prevRenderMatrixPalette.begin(), sMeshC[i].m_prevRenderMatrixPalette.end());
				}

				MeshInstanceData meshInstanceData{};
				meshInstanceData.m_mobility = tc.m_mobility;

				const bool outlined = (outlineC && outlineC[i].m_outlined) || (editorOutlineC && editorOutlineC[i].m_outlined);

				const auto &submeshhandles = meshAsset->getSubMeshhandles();
				const auto &materialAssets = meshAsset->getMaterials();
				for (size_t j = 0; j < submeshhandles.size(); ++j)
				{
					SubMeshInstanceData instanceData{};
					instanceData.m_subMeshHandle = submeshhandles[j];
					instanceData.m_transformIndex = static_cast<uint32_t>(transformIndex);
					instanceData.m_skinningMatricesOffset = static_cast<uint32_t>(skinningMatricesOffset);
					instanceData.m_materialHandle = materialAssets[j]->getMaterialHandle();
					instanceData.m_entityID = entities[i];
					instanceData.m_skinned = skinned;
					instanceData.m_alphaTested = m_materialManager->getMaterial(instanceData.m_materialHandle).m_alphaMode == MaterialAlphaMode::Mask;
					instanceData.m_outlined = outlined;

					// add the index of this submesh instance to the parent mesh instance
					meshInstanceData.m_subMeshInstanceHandles.push_back(static_cast<uint32_t>(m_submeshInstances.size()));

					m_submeshInstances.push_back(instanceData);

					// create transformed bounding sphere of submesh instance
					{
						glm::vec4 bsphere = glm::make_vec4(subMeshDrawInfoTable[instanceData.m_subMeshHandle].m_boundingSphere);
						glm::vec3 pos = bsphereTransform * glm::vec4(glm::vec3(bsphere), 1.0f);
						m_submeshInstanceBoundingSpheres.push_back(glm::vec4(pos, bsphere.w *= boundingSphereScale));
					}
				}

				m_meshInstances.push_back(meshInstanceData);

				// create transformed bounding sphere of mesh instance
				{
					glm::vec4 bsphere{};
					meshAsset->getBoundingSphere(&bsphere.x);
					glm::vec3 pos = bsphereTransform * glm::vec4(glm::vec3(bsphere), 1.0f);
					m_meshInstanceBoundingSpheres.push_back(glm::vec4(pos, bsphere.w *= boundingSphereScale));
				}
			}
		});

	auto createShaderResourceBuffer = [&](gal::DescriptorType descriptorType, auto dataAsVector)
	{
		const size_t elementSize = sizeof(dataAsVector[0]);

		// prepare DescriptorBufferInfo
		gal::DescriptorBufferInfo bufferInfo = gal::Initializers::structuredBufferInfo(elementSize, dataAsVector.size());
		bufferInfo.m_buffer = shaderResourceBufferAllocator->getBuffer();

		// allocate memory
		uint64_t alignment = m_device->getBufferAlignment(descriptorType, elementSize);
		uint8_t *bufferPtr = shaderResourceBufferAllocator->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy to destination
		memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() * elementSize);

		// create a transient bindless handle
		return descriptorType == gal::DescriptorType::STRUCTURED_BUFFER ?
			m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true) :
			m_viewRegistry->createByteBufferViewHandle(bufferInfo, true);
	};

	m_transformsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(gal::DescriptorType::STRUCTURED_BUFFER, m_modelMatrices);
	m_prevTransformsBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(gal::DescriptorType::STRUCTURED_BUFFER, m_prevModelMatrices);
	m_skinningMatricesBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(gal::DescriptorType::STRUCTURED_BUFFER, m_skinningMatrices);
	m_prevSkinningMatricesBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(gal::DescriptorType::STRUCTURED_BUFFER, m_prevSkinningMatrices);
}

void MeshRenderWorld::createMeshRenderList(const glm::mat4 &viewMatrix, const glm::mat4 &viewProjectionMatrix, float farPlane, MeshRenderList2 *result) const noexcept
{
	assert(result);
	*result = {};
	result->m_submeshInstances = m_submeshInstances.data();
	result->m_transformsBufferViewHandle = m_transformsBufferViewHandle;
	result->m_prevTransformsBufferViewHandle = m_prevTransformsBufferViewHandle;
	result->m_skinningMatricesBufferViewHandle = m_skinningMatricesBufferViewHandle;
	result->m_prevSkinningMatricesBufferViewHandle = m_prevSkinningMatricesBufferViewHandle;

	// frustum cull mesh instances
	eastl::vector<uint32_t> survivingMeshInstances(m_meshInstances.size());
	survivingMeshInstances.resize(FrustumCulling::cull(m_meshInstances.size(), nullptr, m_meshInstanceBoundingSpheres.data(), viewProjectionMatrix, survivingMeshInstances.data()));

	// expand mesh instances into submesh instances
	eastl::vector<uint32_t> expandedSubmeshInstances;
	expandedSubmeshInstances.reserve(survivingMeshInstances.size());
	for (auto idx : survivingMeshInstances)
	{
		const auto &meshInstance = m_meshInstances[idx];

		// TODO: implement LOD selection here

		expandedSubmeshInstances.insert(expandedSubmeshInstances.end(), meshInstance.m_subMeshInstanceHandles.begin(), meshInstance.m_subMeshInstanceHandles.end());
	}

	const size_t submeshInstanceCount = expandedSubmeshInstances.size();

	// create array of bounding spheres for culling submesh instances
	eastl::vector<glm::vec4> subMeshBoundingSpheres(submeshInstanceCount);
	for (size_t i = 0; i < submeshInstanceCount; ++i)
	{
		subMeshBoundingSpheres[i] = m_submeshInstanceBoundingSpheres[expandedSubmeshInstances[i]];
	}

	// frustum cull submesh instances
	result->m_indices.clear();
	result->m_indices.resize(submeshInstanceCount);
	result->m_indices.resize(FrustumCulling::cull(submeshInstanceCount, expandedSubmeshInstances.data(), subMeshBoundingSpheres.data(), viewProjectionMatrix, result->m_indices.data()));

	const glm::vec4 viewMatDepthRow = glm::vec4(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2], viewMatrix[3][2]);

	// create sort keys
	eastl::vector<uint64_t> sortKeys;
	sortKeys.reserve(result->m_indices.size());
	for (auto idx : result->m_indices)
	{
		auto createMask = [](uint32_t size) {return size == 64 ? ~uint64_t() : (uint64_t(1) << size) - 1; };

		// key: [listIdx 8][depth 24][instanceIdx 32]

		const auto &submeshInstance = m_submeshInstances[idx];

		const bool dynamic = m_meshInstances[submeshInstance.m_transformIndex].m_mobility == TransformComponent::Mobility::DYNAMIC;
		const auto alphaMode = submeshInstance.m_alphaTested ? MaterialAlphaMode::Mask : MaterialAlphaMode::Opaque;

		const uint64_t listIdx = MeshRenderList2::getListIndex(dynamic, alphaMode, submeshInstance.m_skinned, submeshInstance.m_outlined);

		const float viewSpaceDepth = -glm::dot(viewMatDepthRow, glm::vec4(glm::vec3(m_meshInstanceBoundingSpheres[m_submeshInstances[idx].m_transformIndex]), 1.0f));
		const uint64_t depth = static_cast<uint64_t>(static_cast<double>(glm::clamp(viewSpaceDepth / farPlane, 0.0f, 1.0f)) * createMask(24));

		uint64_t key = 0;
		key |= static_cast<uint64_t>(idx) & createMask(32);
		key |= (depth & createMask(24)) << 32;
		key |= (listIdx & createMask(8)) << 56;

		sortKeys.push_back(key);
	}

	// sort by type and depth
	eastl::sort(sortKeys.begin(), sortKeys.end());

	size_t curListIdx = 0;
	for (size_t i = 0; i < sortKeys.size(); ++i)
	{
		const auto key = sortKeys[i];

		result->m_indices[i] = static_cast<uint32_t>(key); // extract instance idx from key

		size_t listIdx = static_cast<size_t>(key >> 56);
		if (listIdx != curListIdx)
		{
			result->m_offsets[listIdx] = static_cast<uint32_t>(i);
			curListIdx = listIdx;
		}

		++result->m_counts[curListIdx];
	}
}

size_t MeshRenderList2::getListIndex(bool dynamic, MaterialAlphaMode alphaMode, bool skinned, bool outlined) noexcept
{
	size_t index = 0;

	if (outlined)
	{
		index += 12;
	}

	switch (alphaMode)
	{
	case MaterialAlphaMode::Opaque:
		index += 0;
		break;
	case MaterialAlphaMode::Mask:
		index += 4;
		break;
	case MaterialAlphaMode::Blended:
		index += 8;
		break;
	default:
		assert(false);
		break;
	}

	if (dynamic)
	{
		index += 2;
	}

	if (skinned)
	{
		index += 1;
	}

	return index;
}
