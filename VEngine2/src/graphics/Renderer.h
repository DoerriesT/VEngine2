#pragma once
#include "gal/FwdDecl.h"
#include <stdint.h>
#include "Handles.h"
#include "Mesh.h"
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>
#include "RenderData.h"

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
class ECS;

typedef void *ImTextureID;

namespace rg
{
	class RenderGraph;
}

class Renderer
{
public:
	explicit Renderer(void *windowHandle, uint32_t width, uint32_t height) noexcept;
	~Renderer() noexcept;
	void render(float deltaTime, ECS *ecs, uint64_t cameraEntity, float fractionalSimFrameTime) noexcept;
	void resize(uint32_t swapchainWidth, uint32_t swapchainHeight, uint32_t width, uint32_t height) noexcept;
	void getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept;
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
	void setPickingPos(uint32_t x, uint32_t y) noexcept;
	uint64_t getPickedEntity() const noexcept;
	void clearDebugGeometry() noexcept;
	void drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept;
	void drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept;
	void drawDebugBox(const glm::mat4 &transform, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded, bool wireframe = true) noexcept;
	void drawDebugSphere(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept;
	void drawDebugCappedCone(glm::vec3 position, glm::quat rotation, float coneLength, float coneAngle, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept;
	void drawDebugArrow(glm::vec3 position, glm::quat rotation, float scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept;

private:
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
	float m_time = 0.0f;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	RendererResources *m_rendererResources = nullptr;
	MeshManager *m_meshManager = nullptr;
	TextureLoader *m_textureLoader = nullptr;
	TextureManager *m_textureManager = nullptr;
	MaterialManager *m_materialManager = nullptr;
	RenderView *m_renderView = nullptr;
	ImGuiPass *m_imguiPass = nullptr;
	bool m_editorMode = false;
};