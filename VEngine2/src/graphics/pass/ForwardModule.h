#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include "Handles.h"

struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
class VolumetricFogModule;
struct LightRecordData;
struct CommonViewData;

class ForwardModule
{
public:
	struct Data
	{
		const CommonViewData *m_viewData;
		StructuredBufferViewHandle m_transformBufferHandle;
		StructuredBufferViewHandle m_prevTransformBufferHandle;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_prevSkinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		StructuredBufferViewHandle m_globalMediaBufferHandle;
		uint32_t m_globalMediaCount;
		const RenderList *m_renderList;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		const LightRecordData *m_lightRecordData;
		bool m_taaEnabled;
		bool m_ignoreHistory;
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
	gal::GraphicsPipeline *m_forwardPipeline = nullptr;
	gal::GraphicsPipeline *m_forwardSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_skyPipeline = nullptr;
	gal::ComputePipeline *m_gtaoPipeline = nullptr;
	gal::ComputePipeline *m_gtaoBlurPipeline = nullptr;
	gal::GraphicsPipeline *m_indirectLightingPipeline = nullptr;
	VolumetricFogModule *m_volumetricFogModule = nullptr;
};