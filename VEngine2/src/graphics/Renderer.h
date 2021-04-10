#pragma once
#include "gal/FwdDecl.h"
#include <stdint.h>

class BufferStackAllocator;
class RenderView;
class ResourceViewRegistry;

class Renderer
{
public:
	explicit Renderer(void *windowHandle, uint32_t width, uint32_t height);
	~Renderer();
	void render(const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition);
	void resize(uint32_t width, uint32_t height);

private:
	gal::GraphicsDevice *m_device = nullptr;
	gal::Queue *m_graphicsQueue = nullptr;
	gal::SwapChain *m_swapChain = nullptr;
	gal::Semaphore *m_semaphore = nullptr;
	gal::CommandListPool *m_cmdListPools[2] = {};
	gal::CommandList *m_cmdLists[2] = {};
	uint64_t m_semaphoreValue = 0;
	uint64_t m_waitValues[2] = {};
	uint64_t m_frame = 0;
	uint32_t m_width = 1;
	uint32_t m_height = 1;
	gal::ImageView *m_imageViews[3] = {};
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RenderView *m_renderView = nullptr;
};