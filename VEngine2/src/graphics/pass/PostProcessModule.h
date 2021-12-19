#pragma once
#include "graphics/gal/FwdDecl.h"
#include <stdint.h>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include <glm/mat4x4.hpp>
#include "../RenderData.h"

class BufferStackAllocator;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;
struct RenderList;
struct CommonViewData;

class PostProcessModule
{
public:
	struct Data
	{
		const CommonViewData *m_viewData;
		rg::ResourceViewHandle m_lightingImageView;
		rg::ResourceViewHandle m_depthBufferImageViewHandle;
		rg::ResourceViewHandle m_velocityImageViewHandle;
		rg::ResourceViewHandle m_temporalAAResultImageViewHandle;
		rg::ResourceViewHandle m_temporalAAHistoryImageViewHandle;
		rg::ResourceViewHandle m_resultImageViewHandle;
		StructuredBufferViewHandle m_transformBufferHandle;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		const RenderList *m_renderList;
		const RenderList *m_outlineRenderList;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		bool m_debugNormals;
		bool m_renderOutlines;
		bool m_ignoreHistory;
		bool m_taaEnabled;
		bool m_bloomEnabled;
	};

	struct ResultData
	{
		rg::ResourceViewHandle m_tonemappedImageView;
	};

	explicit PostProcessModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(PostProcessModule);
	~PostProcessModule() noexcept;
	void record(rg::RenderGraph *graph, const Data &data, ResultData *resultData) noexcept;

	void clearDebugGeometry() noexcept;
	void drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept;
	void drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::ComputePipeline *m_temporalAAPipeline = nullptr;
	gal::ComputePipeline *m_bloomDownsamplePipeline = nullptr;
	gal::ComputePipeline *m_bloomUpsamplePipeline = nullptr;
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
	gal::GraphicsPipeline *m_debugDrawPipelines[6] = {};
	eastl::vector<DebugDrawVertex> m_debugDrawVertices[6];
};