#pragma once
#include <stdint.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"

struct RenderViewResources;
class ResourceViewRegistry;
class BufferStackAllocator;
class MeshPass;
class GridPass;
class ECS;
class MeshManager;

class RenderView
{
public:
	explicit RenderView(ECS *ecs, gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept;
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
	ECS *m_ecs;
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	MeshManager *m_meshManager = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	RenderViewResources *m_renderViewResources = nullptr;

	MeshPass *m_meshPass = nullptr;
	GridPass *m_gridPass = nullptr;
};