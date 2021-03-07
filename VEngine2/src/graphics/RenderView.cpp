#include "RenderView.h"
#include "pass/GridPass.h"
#include "RenderViewResources.h"
#include "gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>

RenderView::RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_device(device),
	m_width(width),
	m_height(height)
{
	m_renderViewResources = new RenderViewResources(m_device, viewRegistry, m_width, m_height);
	m_gridPass = new GridPass(m_device, offsetBufferSetLayout);
}

RenderView::~RenderView()
{
	delete m_gridPass;
	delete m_renderViewResources;
}

void RenderView::render(gal::CommandList *cmdList, BufferStackAllocator *bufferAllocator, gal::DescriptorSet *offsetBufferSet, const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition, bool transitionResultToTexture) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	{
		gal::Barrier b0 = gal::Initializers::imageBarrier(
			m_renderViewResources->m_resultImage,
			m_renderViewResources->m_resultImagePipelineStages,
			gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
			m_renderViewResources->m_resultImageResourceState,
			gal::ResourceState::WRITE_COLOR_ATTACHMENT);

		cmdList->barrier(1, &b0);

		m_renderViewResources->m_resultImagePipelineStages = gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT;
		m_renderViewResources->m_resultImageResourceState = gal::ResourceState::WRITE_COLOR_ATTACHMENT;
	}

	GridPass::Data gridPassData{};
	gridPassData.m_bufferAllocator = bufferAllocator;
	gridPassData.m_offsetBufferSet = offsetBufferSet;
	gridPassData.m_width = m_width;
	gridPassData.m_height = m_height;
	gridPassData.m_colorAttachment = m_renderViewResources->m_resultImageView;
	gridPassData.m_modelMatrix = glm::mat4(1.0f);
	gridPassData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
	gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
	gridPassData.m_cameraPos = glm::make_vec3(cameraPosition);
	gridPassData.m_cellSize = 1.0f;
	gridPassData.m_gridSize = 1000.0f;

	m_gridPass->record(cmdList, gridPassData);

	{
		gal::Barrier b1 = gal::Initializers::imageBarrier(
			m_renderViewResources->m_resultImage,
			m_renderViewResources->m_resultImagePipelineStages,
			transitionResultToTexture ? gal::PipelineStageFlags::PIXEL_SHADER_BIT : gal::PipelineStageFlags::TRANSFER_BIT,
			m_renderViewResources->m_resultImageResourceState,
			transitionResultToTexture ? gal::ResourceState::READ_RESOURCE : gal::ResourceState::READ_TRANSFER);

		cmdList->barrier(1, &b1);

		m_renderViewResources->m_resultImagePipelineStages = transitionResultToTexture ? gal::PipelineStageFlags::PIXEL_SHADER_BIT : gal::PipelineStageFlags::TRANSFER_BIT;
		m_renderViewResources->m_resultImageResourceState = transitionResultToTexture ? gal::ResourceState::READ_RESOURCE : gal::ResourceState::READ_TRANSFER;
	}
}

void RenderView::resize(uint32_t width, uint32_t height) noexcept
{
	if (width != 0 && height != 0)
	{
		m_width = width;
		m_height = height;
		m_renderViewResources->resize(width, height);
	}
}

gal::Image *RenderView::getResultImage() const noexcept
{
	return m_renderViewResources->m_resultImage;
}

gal::ImageView *RenderView::getResultImageView() const noexcept
{
	return m_renderViewResources->m_resultImageView;
}

TextureViewHandle RenderView::getResultTextureViewHandle() const noexcept
{
	return m_renderViewResources->m_resultImageTextureViewHandle;
}
