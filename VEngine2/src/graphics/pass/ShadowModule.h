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

class ShadowModule
{
public:
	struct Data
	{
		void *m_profilingCtx;
		BufferStackAllocator *m_bufferAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		gal::DescriptorSet *m_bindlessSet;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		size_t m_shadowJobCount;
		const glm::mat4 *m_shadowMatrices;
		const rg::ResourceViewHandle *m_shadowTextureViewHandles;
		const RenderList *m_renderList;
		const glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
	};

	explicit ShadowModule(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(ShadowModule);
	~ShadowModule() noexcept;
	void record(rg::RenderGraph *graph, const Data &data) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_shadowPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedAlphaTestedPipeline = nullptr;
};