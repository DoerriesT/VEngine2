#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>
#include "../RenderGraph.h"
#include "utility/DeletedCopyMove.h"
#include "Handles.h"

class BufferStackAllocator;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;

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
		uint32_t m_meshCount;
		uint32_t m_skinnedMeshCount;
		uint32_t m_skinningMatrixBufferIndex;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 *m_modelMatrices;
		glm::vec3 m_cameraPosition;
		StructuredBufferViewHandle m_materialsBufferHandle;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		const MaterialHandle *m_materialHandles;
		const uint32_t *m_skinningMatrixOffsets;
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
	gal::GraphicsPipeline *m_forwardPipeline = nullptr;
	gal::GraphicsPipeline *m_forwardSkinnedPipeline = nullptr;
};