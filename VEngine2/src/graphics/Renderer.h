#pragma once
#include "gal/FwdDecl.h"
#include <stdint.h>
#include "Handles.h"

class BufferStackAllocator;
class RenderView;
class ResourceViewRegistry;
class RendererResources;
class TextureLoader;
class TextureManager;
class ImGuiPass;

typedef void *ImTextureID;

class Renderer
{
public:
	explicit Renderer(void *windowHandle, uint32_t width, uint32_t height);
	~Renderer();
	void render(const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition);
	void resize(uint32_t width, uint32_t height);
	TextureHandle loadTexture(size_t fileSize, const char *fileData, const char *textureName) noexcept;
	void destroyTexture(TextureHandle handle) noexcept;
	ImTextureID getImGuiTextureID(TextureHandle handle) noexcept;

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
	RendererResources *m_rendererResources = nullptr;
	TextureLoader *m_textureLoader = nullptr;
	TextureManager *m_textureManager = nullptr;
	RenderView *m_renderView = nullptr;
	ImGuiPass *m_imguiPass = nullptr;
};