#pragma once
#include "RenderData.h"
#include "Material.h"
#include "ViewHandles.h"
#include "gal/FwdDecl.h"

class ECS;
class MaterialManager;
class MeshManager;
class ResourceViewRegistry;
class LinearGPUBufferAllocator;

struct MeshRenderList2
{
	uint32_t m_offsets[24];
	uint32_t m_counts[24];
	eastl::vector<uint32_t> m_indices;
	const SubMeshInstanceData *m_submeshInstances;
	StructuredBufferViewHandle m_transformsBufferViewHandle;
	StructuredBufferViewHandle m_prevTransformsBufferViewHandle;
	StructuredBufferViewHandle m_skinningMatricesBufferViewHandle;
	StructuredBufferViewHandle m_prevSkinningMatricesBufferViewHandle;

	static size_t getListIndex(bool dynamic, MaterialAlphaMode alphaMode, bool skinned, bool outlined) noexcept;
};

class MeshRenderWorld
{
public:
	explicit MeshRenderWorld(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager) noexcept;
	void update(ECS *ecs, LinearGPUBufferAllocator *shaderResourceBufferAllocator) noexcept;
	void createMeshRenderList(const glm::mat4 &viewMatrix, const glm::mat4 &viewProjectionMatrix, float farPlane, MeshRenderList2 *result) const noexcept;

private:
	gal::GraphicsDevice *m_device;
	ResourceViewRegistry *m_viewRegistry;
	MeshManager *m_meshManager;
	MaterialManager *m_materialManager;
	eastl::vector<glm::vec4> m_meshInstanceBoundingSpheres;
	eastl::vector<glm::vec4> m_submeshInstanceBoundingSpheres;
	eastl::vector<MeshInstanceData> m_meshInstances;
	eastl::vector<SubMeshInstanceData> m_submeshInstances;
	eastl::vector<glm::mat4> m_modelMatrices;
	eastl::vector<glm::mat4> m_prevModelMatrices;
	eastl::vector<glm::mat4> m_skinningMatrices;
	eastl::vector<glm::mat4> m_prevSkinningMatrices;
	StructuredBufferViewHandle m_transformsBufferViewHandle;
	StructuredBufferViewHandle m_prevTransformsBufferViewHandle;
	StructuredBufferViewHandle m_skinningMatricesBufferViewHandle;
	StructuredBufferViewHandle m_prevSkinningMatricesBufferViewHandle;
};