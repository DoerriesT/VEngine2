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
		BufferStackAllocator *m_bufferAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		gal::DescriptorSet *m_bindlessSet;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_meshCount;
		gal::ImageView *m_colorAttachment;
		gal::ImageView *m_depthBufferAttachment;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
	};

	explicit MeshPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout);
	~MeshPass();
	void record(gal::CommandList *cmdList, const Data &data);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_pipeline = nullptr;
};