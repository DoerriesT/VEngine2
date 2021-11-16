#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include "ViewHandles.h"
#include "RenderGraph.h"

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
	rg::ResourceStateData m_resultImageState[1] = {};

	gal::Buffer *m_skinningMatricesBuffers[2] = {};
	StructuredBufferViewHandle m_skinningMatricesBufferViewHandles[2] = {};

	gal::Buffer *m_exposureDataBuffer = nullptr;
	rg::ResourceStateData m_exposureDataBufferState[1] = {};

	explicit RenderViewResources(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, uint32_t width, uint32_t height) noexcept;
	~RenderViewResources();
	void resize(uint32_t width, uint32_t height) noexcept;

private:
	void create(uint32_t width, uint32_t height) noexcept;
	void destroy() noexcept;
};