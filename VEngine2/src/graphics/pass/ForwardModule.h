#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include "Handles.h"

class BufferStackAllocator;
struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
struct PunctualLightGPU;
class RendererResources;
class VolumetricFogModule;

class ForwardModule
{
public:
	struct Data
	{
		void *m_profilingCtx;
		BufferStackAllocator *m_bufferAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		gal::DescriptorSet *m_bindlessSet;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_frame;
		float m_fovy;
		float m_nearPlane;
		uint32_t m_pickingPosX;
		uint32_t m_pickingPosY;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		StructuredBufferViewHandle m_globalMediaBufferHandle;
		StructuredBufferViewHandle m_directionalLightsBufferHandle;
		StructuredBufferViewHandle m_directionalLightsShadowedBufferHandle;
		StructuredBufferViewHandle m_lightTransformBufferHandle;
		StructuredBufferViewHandle m_punctualLightsBufferHandle;
		ByteBufferViewHandle m_punctualLightsDepthBinsBufferHandle;
		rg::ResourceViewHandle m_pickingBufferHandle;
		rg::ResourceViewHandle m_exposureBufferHandle;
		const rg::ResourceViewHandle *m_shadowMapViewHandles;
		uint32_t m_globalMediaCount;
		uint32_t m_directionalLightCount;
		uint32_t m_directionalLightShadowedCount;
		uint32_t m_punctualLightCount;
		size_t m_shadowMapViewHandleCount;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 m_invViewProjectionMatrix;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_invProjectionMatrix;
		glm::vec3 m_cameraPosition;
		const RenderList *m_renderList;
		const glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		const PunctualLightGPU *m_punctualLights;
		const uint64_t *m_punctualLightsOrder;
		RendererResources *m_rendererResources;
	};

	struct ResultData
	{
		rg::ResourceViewHandle m_depthBufferImageViewHandle;
		rg::ResourceViewHandle m_lightingImageViewHandle;
		rg::ResourceViewHandle m_normalRoughnessImageViewHandle;
		rg::ResourceViewHandle m_albedoMetalnessImageViewHandle;
		rg::ResourceViewHandle m_velocityImageViewHandle;
	};

	explicit ForwardModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(ForwardModule);
	~ForwardModule() noexcept;
	void record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_depthPrepassPipeline = nullptr;
	gal::GraphicsPipeline *m_depthPrepassSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_depthPrepassAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_depthPrepassSkinnedAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_lightTileAssignmentPipeline = nullptr;
	gal::GraphicsPipeline *m_forwardPipeline = nullptr;
	gal::GraphicsPipeline *m_forwardSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_skyPipeline = nullptr;
	gal::ComputePipeline *m_gtaoPipeline = nullptr;
	gal::ComputePipeline *m_gtaoBlurPipeline = nullptr;
	gal::GraphicsPipeline *m_indirectLightingPipeline = nullptr;
	VolumetricFogModule *m_volumetricFogModule = nullptr;
};