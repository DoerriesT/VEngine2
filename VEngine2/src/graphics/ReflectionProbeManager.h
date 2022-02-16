#pragma once
#include <EASTL/fixed_vector.h>
#include "utility/DeletedCopyMove.h"
#include "RenderData.h"
#include "RenderGraph.h"

class ECS;
class RendererResources;
struct SubMeshDrawInfo;
class MeshRenderWorld;
struct SubMeshBufferHandles;
struct LightRecordData;
class ResourceViewRegistry;
class LinearGPUBufferAllocator;

class ReflectionProbeManager
{
public:
	static constexpr size_t k_cacheSize = 128;
	static constexpr size_t k_resolution = 256;
	static constexpr size_t k_numPrefilterMips = 5;
	static constexpr size_t k_numMips = 4;

	struct Data
	{
		ECS *m_ecs;
		const glm::vec3 *m_cameraPosition;
		void *m_gpuProfilingCtx;
		LinearGPUBufferAllocator *m_shaderResourceLinearAllocator;
		LinearGPUBufferAllocator *m_constantBufferLinearAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		StructuredBufferViewHandle m_materialsBufferHandle;
		StructuredBufferViewHandle m_irradianceVolumeBufferViewHandle;
		uint32_t m_irradianceVolumeCount;
		const MeshRenderWorld *m_meshRenderWorld;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
	};

	explicit ReflectionProbeManager(gal::GraphicsDevice *device, RendererResources *renderResources, ResourceViewRegistry *viewRegistry) noexcept;
	DELETED_COPY_MOVE(ReflectionProbeManager);
	~ReflectionProbeManager() noexcept;
	
	void update(rg::RenderGraph *graph, const Data &data) noexcept;
	TextureViewHandle getReflectionProbeArrayTextureViewHandle() const noexcept;
	StructuredBufferViewHandle getReflectionProbeDataBufferhandle() const noexcept;
	uint32_t getReflectionProbeCount() const noexcept;
	void invalidateAllReflectionProbes() noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RendererResources *m_rendererResources = nullptr;
	gal::GraphicsPipeline *m_probeGbufferPipeline = nullptr;
	gal::GraphicsPipeline *m_probeGbufferAlphaTestedPipeline = nullptr;
	gal::ComputePipeline *m_probeRelightPipeline = nullptr;
	gal::ComputePipeline *m_ffxDownsamplePipeline = nullptr;
	gal::ComputePipeline *m_probeFilterPipeline = nullptr;

	gal::Image *m_probeDepthBufferImage = nullptr;
	gal::ImageView *m_probeDepthArrayView = {};
	gal::ImageView *m_probeDepthSliceViews[6] = {};

	gal::Image *m_probeAlbedoRoughnessArrayImage = nullptr;
	gal::ImageView *m_probeAlbedoRoughnessArrayView = {};
	gal::ImageView *m_probeAlbedoRoughnessSliceViews[k_cacheSize * 6] = {};
	TextureViewHandle m_probeAlbedoRoughnessTextureViewHandle = {};

	gal::Image *m_probeNormalDepthArrayImage = nullptr;
	gal::ImageView *m_probeNormalDepthArrayView = {};
	gal::ImageView *m_probeNormalDepthSliceViews[k_cacheSize * 6] = {};
	TextureViewHandle m_probeNormalDepthTextureViewHandle = {};

	gal::Image *m_probeTmpLitImage;
	gal::ImageView *m_probeTmpLitArrayViews[k_numPrefilterMips] = {}; // one per mip
	TextureViewHandle m_probeTmpLitArrayMip0TextureViewHandle = {};
	RWTextureViewHandle m_probeTmpLitArrayRWTextureViewHandles[k_numPrefilterMips] = {}; // one per mip
	gal::ImageView *m_probeTmpLitCubeView = {};
	TextureViewHandle m_probeTmpLitCubeTextureViewHandle = {};

	gal::Image *m_probeArrayImage;
	gal::ImageView *m_probeArrayView = {};
	gal::ImageView *m_probeArrayCubeView = {};
	TextureViewHandle m_probeArrayCubeTextureViewHandle = {};
	RWTextureViewHandle m_probeArrayRWTextureViewHandle = {};
	gal::ImageView *m_probeArrayMipViews[k_cacheSize][k_numMips] = {};
	RWTextureViewHandle m_probeArrayMipRWTextureViewHandles[k_cacheSize][k_numMips] = {};

	gal::Buffer *m_spdCounterBuffer = {};
	RWByteBufferViewHandle m_spdCounterBufferViewHandle = {};

	StructuredBufferViewHandle m_reflectionProbesBufferHandle = {};
	uint32_t m_reflectionProbeCount = 0;

	eastl::fixed_vector<uint32_t, k_cacheSize> m_freeCacheSlots;
	EntityID m_cacheSlotOwners[k_cacheSize] = {};
	uint32_t m_internalFrame = 0;
	bool m_invalidateAllProbes = false;;
	glm::mat4 m_curRenderProbeViewProjMatrices[6] = {};
	glm::mat4 m_curRenderProbeViewMatrices[6] = {};
	glm::mat4 m_curRelightProbeWorldToLocalTransposed = {};
	glm::vec3 m_curRelightProbePosition = {};
	float m_curRelightProbeNearPlane = 0.5f;
	float m_curRelightProbeFarPlane = 50.0f;
	float m_curRenderProbeFarPlane = 50.0f;

	uint32_t allocateCacheSlot(EntityID entity) noexcept;
	void freeCacheSlot(uint32_t slot) noexcept;

	void renderProbeGBuffer(rg::RenderGraph *graph, const Data &data, size_t probeIdx) const noexcept;
	void relightProbe(rg::RenderGraph *graph, const Data &data, size_t probeIdx) const noexcept;
};