#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>

class BufferStackAllocator;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;

class MeshPass
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
		gal::ImageView *m_colorAttachment;
		gal::ImageView *m_depthBufferAttachment;
		uint32_t m_skinningMatrixBufferIndex;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
		const uint32_t *m_skinningMatrixOffsets;
	};

	explicit MeshPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout);
	~MeshPass();
	void record(gal::CommandList *cmdList, const Data &data);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_pipeline = nullptr;
	gal::GraphicsPipeline *m_skinnedPipeline = nullptr;
};