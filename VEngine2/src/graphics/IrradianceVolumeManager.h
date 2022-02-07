#pragma once
#include "utility/DeletedCopyMove.h"
#include "RenderGraph.h"
#include "ecs/ECSCommon.h"
#include "component/TransformComponent.h"
#include <EASTL/vector.h>
#include <EASTL/queue.h>
#include "RenderData.h"
#include "LightManager.h"

class ECS;
class RendererResources;
struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
struct LightRecordData;
class ResourceViewRegistry;
class LinearGPUBufferAllocator;
class LightManager;

class IrradianceVolumeManager
{
public:
	static constexpr uint32_t k_renderResolution = 128;
	static constexpr uint32_t k_diffuseProbeResolution = 8;
	static constexpr uint32_t k_visibilityProbeResolution = 16;

	struct Data
	{
		ECS *m_ecs;
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
		uint32_t m_frame;
	};

	explicit IrradianceVolumeManager(gal::GraphicsDevice *device, RendererResources *renderResources, ResourceViewRegistry *viewRegistry) noexcept;
	DELETED_COPY_MOVE(IrradianceVolumeManager);
	~IrradianceVolumeManager() noexcept;

	void update(rg::RenderGraph *graph, const Data &data) noexcept;
	StructuredBufferViewHandle getIrradianceVolumeBufferViewHandle() const noexcept;
	uint32_t getIrradianceVolumeCount() const noexcept;
	bool startBake() noexcept;
	bool abortBake() noexcept;
	bool isBaking() const noexcept;
	uint32_t getProcessedProbesCount() const noexcept;
	uint32_t getTotalProbeCount() const noexcept;

private:
	static constexpr uint32_t k_tmpTextureProbeWidth = 256;
	static constexpr uint32_t k_tmpTextureProbeHeight = 64;

	struct InternalIrradianceVolume
	{
		Transform m_transform;
		EntityID m_entity;
		gal::Image *m_diffuseImage = nullptr;
		gal::Image *m_visibilityImage = nullptr;
		gal::Image *m_averageDiffuseImage = nullptr;
		gal::ImageView *m_diffuseImageView = nullptr;
		gal::ImageView *m_visibilityImageView = nullptr;
		gal::ImageView *m_averageDiffuseImageView = nullptr;
		TextureViewHandle m_diffuseImageViewHandle = {};
		TextureViewHandle m_visibilityImageViewHandle = {};
		TextureViewHandle m_averageDiffuseImageViewHandle = {};
		RWTextureViewHandle m_averageDiffuseImageRWViewHandle = {};

		uint32_t m_resolutionX;
		uint32_t m_resolutionY;
		uint32_t m_resolutionZ;
		float m_fadeoutStart;
		float m_fadeoutEnd;
		float m_selfShadowBias;
		float m_nearPlane;
		float m_farPlane;
	};

	struct DeletionQueueItem
	{
		gal::Image *m_diffuseImage = nullptr;
		gal::Image *m_visibilityImage = nullptr;
		gal::Image *m_averageDiffuseImage = nullptr;
		gal::ImageView *m_diffuseImageView = nullptr;
		gal::ImageView *m_visibilityImageView = nullptr;
		gal::ImageView *m_averageDiffuseImageView = nullptr;
		TextureViewHandle m_diffuseImageViewHandle = {};
		TextureViewHandle m_visibilityImageViewHandle = {};
		TextureViewHandle m_averageDiffuseImageViewHandle = {};
		RWTextureViewHandle m_averageDiffuseImageRWViewHandle = {};
		uint64_t m_frameToFreeIn = UINT64_MAX;
	};

	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RendererResources *m_rendererResources = nullptr;
	LightManager *m_lightManager = nullptr;
	gal::GraphicsPipeline *m_forwardPipeline = nullptr;
	gal::GraphicsPipeline *m_forwardAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_skyPipeline = nullptr;
	gal::ComputePipeline *m_diffuseFilterPipeline = nullptr;
	gal::ComputePipeline *m_visibilityFilterPipeline = nullptr;
	gal::ComputePipeline *m_averageFilterPipeline = nullptr;

	gal::Image *m_colorImage = nullptr;
	rg::ResourceStateData m_colorImageState[6] = {};

	gal::Image *m_distanceImage = nullptr;
	rg::ResourceStateData m_distanceImageState[6] = {};

	gal::Image *m_depthBufferImage = nullptr;
	gal::ImageView *m_depthBufferImageView = nullptr;

	gal::Image *m_tmpIrradianceVolumeDiffuseImage = nullptr;
	rg::ResourceStateData m_tmpIrradianceVolumeDiffuseImageState = {};

	gal::Image *m_tmpIrradianceVolumeVisibilityImage = nullptr;
	rg::ResourceStateData m_tmpIrradianceVolumeVisibilityImageState = {};

	StructuredBufferViewHandle m_irradianceVolumeBufferHandle = {};

	LightRecordData m_lightRecordData[6] = {};

	bool m_baking = false;
	bool m_abortedBake = false;
	bool m_startedBake = false;
	eastl::vector<InternalIrradianceVolume> m_volumes;
	eastl::vector<IrradianceVolumeGPU> m_sortedGPUVolumes;
	eastl::queue<DeletionQueueItem> m_deletionQueue;

	uint32_t m_curBounce = 0;
	uint32_t m_curVolumeIdx = 0;
	uint32_t m_curProbeIdx = 0;
	uint32_t m_processedProbes = 0;
	uint32_t m_totalProbes = 0;
	uint32_t m_targetBounces = 3;
};