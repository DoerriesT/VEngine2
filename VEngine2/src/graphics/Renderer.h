#pragma once
#include "gal/FwdDecl.h"
#include <stdint.h>
#include "ecs/ECS.h"
#include "Handles.h"
#include "Mesh.h"

#ifdef OPAQUE
#undef OPAQUE
#endif

#include "Material.h"

class BufferStackAllocator;
class RenderView;
class ResourceViewRegistry;
class RendererResources;
class MeshManager;
class TextureLoader;
class TextureManager;
class MaterialManager;
class ImGuiPass;

typedef void *ImTextureID;

namespace rg
{
	class RenderGraph;
}

class Renderer
{
public:
	explicit Renderer(ECS *ecs, void *windowHandle, uint32_t width, uint32_t height) noexcept;
	~Renderer() noexcept;
	void render() noexcept;
	void resize(uint32_t swapchainWidth, uint32_t swapchainHeight, uint32_t width, uint32_t height) noexcept;
	void getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept;
	void setCameraEntity(EntityID cameraEntity) noexcept;
	EntityID getCameraEntity() const noexcept;
	void createSubMeshes(uint32_t count, SubMeshCreateInfo *subMeshes, SubMeshHandle *handles) noexcept;
	void destroySubMeshes(uint32_t count, SubMeshHandle *handles) noexcept;
	TextureHandle loadTexture(size_t fileSize, const char *fileData, const char *textureName) noexcept;
	TextureHandle loadRawRGBA8(size_t fileSize, const char *fileData, const char *textureName, uint32_t width, uint32_t height) noexcept;
	void destroyTexture(TextureHandle handle) noexcept;
	void createMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void updateMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept;
	void destroyMaterials(uint32_t count, MaterialHandle *handles) noexcept;
	ImTextureID getImGuiTextureID(TextureHandle handle) noexcept;
	ImTextureID getEditorViewportTextureID() noexcept;
	void setEditorMode(bool editorMode) noexcept;
	bool isEditorMode() const noexcept;

private:
	ECS *m_ecs = nullptr;
	gal::GraphicsDevice *m_device = nullptr;
	gal::SwapChain *m_swapChain = nullptr;
	gal::Semaphore *m_semaphores[3] = {};
	uint64_t m_semaphoreValues[3] = {};
	rg::RenderGraph *m_renderGraph = nullptr;
	uint64_t m_frame = 0;
	uint32_t m_swapchainWidth = 1;
	uint32_t m_swapchainHeight = 1;
	uint32_t m_width = 1;
	uint32_t m_height = 1;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RendererResources *m_rendererResources = nullptr;
	MeshManager *m_meshManager = nullptr;
	TextureLoader *m_textureLoader = nullptr;
	TextureManager *m_textureManager = nullptr;
	MaterialManager *m_materialManager = nullptr;
	RenderView *m_renderView = nullptr;
	ImGuiPass *m_imguiPass = nullptr;
	EntityID m_cameraEntity = k_nullEntity;
	bool m_editorMode = false;
};