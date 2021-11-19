#pragma once
#include "graphics/gal/FwdDecl.h"
#include <stdint.h>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include <glm/mat4x4.hpp>

class BufferStackAllocator;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;
struct RenderList;

class PostProcessModule
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
		float m_time;
		float m_deltaTime;
		rg::ResourceViewHandle m_lightingImageView;
		rg::ResourceViewHandle m_depthBufferImageViewHandle;
		rg::ResourceViewHandle m_resultImageViewHandle;
		rg::ResourceViewHandle m_exposureBufferViewHandle;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		glm::mat4 m_viewProjectionMatrix;
		glm::vec3 m_cameraPosition;
		const RenderList *m_renderList;
		const RenderList *m_outlineRenderList;
		const glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		bool m_debugNormals;
		bool m_renderOutlines;
	};

	struct ResultData
	{
		rg::ResourceViewHandle m_tonemappedImageView;
	};

	explicit PostProcessModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(PostProcessModule);
	~PostProcessModule() noexcept;
	void record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::ComputePipeline *m_luminanceHistogramPipeline = nullptr;
	gal::ComputePipeline *m_autoExposurePipeline = nullptr;
	gal::ComputePipeline *m_tonemapPipeline = nullptr;
	gal::GraphicsPipeline *m_outlineIDPipeline = nullptr;
	gal::GraphicsPipeline *m_outlineIDSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_outlineIDAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_outlineIDSkinnedAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_outlinePipeline = nullptr;
	gal::GraphicsPipeline *m_debugNormalsPipeline = nullptr;
	gal::GraphicsPipeline *m_debugNormalsSkinnedPipeline = nullptr;
};