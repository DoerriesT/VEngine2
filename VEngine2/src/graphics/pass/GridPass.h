#pragma once
#include "graphics/gal/FwdDecl.h"
#include <glm/mat4x4.hpp>

class BufferStackAllocator;

class GridPass
{
public:
	struct Data
	{
		void *m_profilingCtx;
		BufferStackAllocator *m_bufferAllocator;
		gal::DescriptorSet *m_offsetBufferSet;
		uint32_t m_width;
		uint32_t m_height;
		gal::ImageView *m_colorAttachment;
		gal::ImageView *m_depthBufferAttachment;
		glm::mat4 m_modelMatrix;
		glm::mat4 m_viewProjectionMatrix;
		glm::vec4 m_thinLineColor;
		glm::vec4 m_thickLineColor;
		glm::vec3 m_cameraPos;
		float m_cellSize;
		float m_gridSize;
	};

	explicit GridPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout);
	~GridPass();
	void record(gal::CommandList *cmdList, const Data &data);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::GraphicsPipeline *m_pipeline = nullptr;
};