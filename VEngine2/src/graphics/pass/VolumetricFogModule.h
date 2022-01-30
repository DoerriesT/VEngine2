#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include "Handles.h"

class LinearGPUBufferAllocator;
struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
struct PunctualLightGPU;
class RendererResources;
struct LightRecordData;
struct CommonViewData;

class VolumetricFogModule
{
public:
	struct Data
	{
		const CommonViewData *m_viewData;
		StructuredBufferViewHandle m_globalMediaBufferHandle;
		StructuredBufferViewHandle m_localMediaBufferHandle;
		ByteBufferViewHandle m_localMediaDepthBinsBufferHandle;
		rg::ResourceViewHandle m_localMediaTileTextureViewHandle;
		StructuredBufferViewHandle m_irradianceVolumeBufferHandle;
		uint32_t m_globalMediaCount;
		uint32_t m_localMediaCount;
		uint32_t m_irradianceVolumeCount;
		bool m_ignoreHistory;
		const LightRecordData *m_lightRecordData;
	};

	struct ResultData
	{
		rg::ResourceViewHandle m_volumetricFogImageViewHandle;
	};

	explicit VolumetricFogModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(VolumetricFogModule);
	~VolumetricFogModule() noexcept;
	void record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::ComputePipeline *m_scatterPipeline = nullptr;
	gal::ComputePipeline *m_filterPipeline = nullptr;
	gal::ComputePipeline *m_integratePipeline = nullptr;
	gal::Image *m_historyTextures[2] = {};
	rg::ResourceStateData m_historyTextureState[2] = {};
	float *m_haltonJitter = nullptr;
	glm::mat4 m_prevViewMatrix;
	glm::mat4 m_prevProjMatrix;
};