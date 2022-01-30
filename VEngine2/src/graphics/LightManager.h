#pragma once
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>
#include "utility/DeletedCopyMove.h"
#include "RenderData.h"
#include "RenderGraph.h"

class RenderGraph;
struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
class ECS;
struct TransformComponent;
struct LightComponent;
class LinearGPUBufferAllocator;
class RendererResources;

struct LightRecordData
{
	uint32_t m_directionalLightCount;
	uint32_t m_directionalLightShadowedCount;
	uint32_t m_punctualLightCount;
	uint32_t m_punctualLightShadowedCount;
	StructuredBufferViewHandle m_directionalLightsBufferViewHandle;
	StructuredBufferViewHandle m_directionalLightsShadowedBufferViewHandle;
	StructuredBufferViewHandle m_punctualLightsBufferViewHandle;
	rg::ResourceViewHandle m_punctualLightsTileTextureViewHandle;
	ByteBufferViewHandle m_punctualLightsDepthBinsBufferViewHandle;
	StructuredBufferViewHandle m_punctualLightsShadowedBufferViewHandle;
	rg::ResourceViewHandle m_punctualLightsShadowedTileTextureViewHandle;
	ByteBufferViewHandle m_punctualLightsShadowedDepthBinsBufferViewHandle;
	eastl::vector<rg::ResourceViewHandle> m_shadowMapViewHandles;
};

class LightManager
{
public:
	struct Data
	{
		ECS *m_ecs;
		uint32_t m_width;
		uint32_t m_height;
		float m_near;
		float m_far;
		float m_fovy;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 m_invViewMatrix;
		glm::vec4 m_viewMatrixDepthRow;
		void *m_gpuProfilingCtx;
		LinearGPUBufferAllocator *m_shaderResourceLinearAllocator;
		LinearGPUBufferAllocator *m_constantBufferLinearAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		StructuredBufferViewHandle m_transformBufferHandle;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		const RenderList *m_renderList;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
	};


	explicit LightManager(gal::GraphicsDevice *device, RendererResources *renderResources, ResourceViewRegistry *viewRegistry) noexcept;
	DELETED_COPY_MOVE(LightManager);
	~LightManager() noexcept;
	void update(const Data &data, rg::RenderGraph *graph, LightRecordData *resultLightRecordData) noexcept;

private:
	gal::GraphicsDevice *m_device;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RendererResources *m_rendererResources = nullptr;
	eastl::vector<uint32_t> m_tmpDepthBinMemory;

	gal::GraphicsPipeline *m_lightTileAssignmentPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedAlphaTestedPipeline = nullptr;
};