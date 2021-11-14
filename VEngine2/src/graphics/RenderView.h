#pragma once
#include <stdint.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"
#include "RenderGraph.h"
#include <glm/mat4x4.hpp>
#include "RenderData.h"

struct RenderViewResources;
class ResourceViewRegistry;
class BufferStackAllocator;
class ForwardModule;
class PostProcessModule;
class GridPass;
class ECS;
class MeshManager;
class MaterialManager;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;
enum MaterialHandle : uint32_t;
class RendererResources;

class RenderView
{
public:
	explicit RenderView(ECS *ecs, gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *renderResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept;
	~RenderView();
	void render(
		rg::RenderGraph *graph,
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
	rg::ResourceViewHandle getResultRGViewHandle() const noexcept;

private:
	ECS *m_ecs;
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	MeshManager *m_meshManager = nullptr;
	MaterialManager *m_materialManager = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_frame = 0;
	RendererResources *m_rendererResources = nullptr;
	RenderViewResources *m_renderViewResources = nullptr;
	rg::ResourceViewHandle m_resultImageViewHandle = {};

	ForwardModule *m_forwardModule = nullptr;
	PostProcessModule *m_postProcessModule = nullptr;
	GridPass *m_gridPass = nullptr;

	eastl::vector<glm::mat4> m_modelMatrices;
	eastl::vector<DirectionalLightGPU> m_directionalLights;
	RenderList m_renderList;
};