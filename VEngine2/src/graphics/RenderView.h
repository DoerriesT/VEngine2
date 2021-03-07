#pragma once
#include <stdint.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"

struct RenderViewResources;
class ResourceViewRegistry;
class BufferStackAllocator;
class GridPass;

class RenderView
{
public:
	explicit RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept;
	~RenderView();
	void render(
		gal::CommandList *cmdList, 
		BufferStackAllocator *bufferAllocator, 
		gal::DescriptorSet *offsetBufferSet,
		const float *viewMatrix, 
		const float *projectionMatrix, 
		const float *cameraPosition, 
		bool transitionResultToTexture) noexcept;
	void resize(uint32_t width, uint32_t height) noexcept;
	gal::Image *getResultImage() const noexcept;
	gal::ImageView *getResultImageView() const noexcept;
	TextureViewHandle getResultTextureViewHandle() const noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	RenderViewResources *m_renderViewResources = nullptr;

	GridPass *m_gridPass = nullptr;
};