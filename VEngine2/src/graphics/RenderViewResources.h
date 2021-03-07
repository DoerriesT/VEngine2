#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include "ViewHandles.h"

class ResourceViewRegistry;

struct RenderViewResources
{
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;

	uint32_t m_width = 0;
	uint32_t m_height = 0;

	gal::Image *m_resultImage = nullptr;
	gal::ImageView *m_resultImageView = nullptr;
	TextureViewHandle m_resultImageTextureViewHandle = {};
	gal::ResourceState m_resultImageResourceState = gal::ResourceState::UNDEFINED;
	gal::PipelineStageFlags m_resultImagePipelineStages = gal::PipelineStageFlags::TOP_OF_PIPE_BIT;

	explicit RenderViewResources(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, uint32_t width, uint32_t height) noexcept;
	~RenderViewResources();
	void resize(uint32_t width, uint32_t height) noexcept;

private:
	void create(uint32_t width, uint32_t height) noexcept;
	void destroy() noexcept;
};