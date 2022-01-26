#pragma once
#include <stdint.h>
#include "gal/FwdDecl.h"
#include "ViewHandles.h"
#include "RenderGraph.h"
#include <glm/mat4x4.hpp>
#include "RenderData.h"
#include "CommonViewData.h"

struct RenderViewResources;
class ResourceViewRegistry;
class LinearGPUBufferAllocator;
class ShadowModule;
class ForwardModule;
class PostProcessModule;
class GridPass;
class MeshManager;
class MaterialManager;
struct SubMeshDrawInfo;
struct SubMeshBufferHandles;
enum MaterialHandle : uint32_t;
class RendererResources;
struct Transform;
struct CameraComponent;
class LightManager;
class ECS;

class RenderView
{
public:
	struct EffectSettings
	{
		bool m_postProcessingEnabled;
		bool m_renderEditorGrid;
		bool m_renderDebugNormals;
		bool m_taaEnabled;
		bool m_sharpenEnabled;
		bool m_bloomEnabled;
		bool m_pickingEnabled;
		bool m_autoExposureEnabled;
		float m_manualExposure;
	};

	struct Data
	{
		EffectSettings m_effectSettings;
		ECS *m_ecs;
		float m_deltaTime;
		float m_time;
		TextureViewHandle m_reflectionProbeCacheTextureViewHandle;
		StructuredBufferViewHandle m_reflectionProbeDataBufferHandle;
		uint32_t m_reflectionProbeCount;
		StructuredBufferViewHandle m_transformsBufferViewHandle;
		StructuredBufferViewHandle m_prevTransformsBufferViewHandle;
		StructuredBufferViewHandle m_skinningMatricesBufferViewHandle;
		StructuredBufferViewHandle m_prevSkinningMatricesBufferViewHandle;
		StructuredBufferViewHandle m_globalMediaBufferViewHandle;
		uint32_t m_globalMediaCount;
		const RenderList *m_renderList;
		const RenderList *m_outlineRenderList;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
		glm::vec3 m_cameraPosition;
		float m_nearPlane;
		float m_farPlane;
		float m_fovy;
		uint32_t m_renderResourceIdx;
		uint32_t m_prevRenderResourceIdx;
		bool m_ignoreHistory;
	};

	explicit RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *renderResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept;
	~RenderView();
	void render(const Data &data, rg::RenderGraph *graph) noexcept;
	void resize(uint32_t width, uint32_t height) noexcept;
	void setPickingPos(uint32_t x, uint32_t y) noexcept;
	gal::Image *getResultImage() const noexcept;
	gal::ImageView *getResultImageView() const noexcept;
	TextureViewHandle getResultTextureViewHandle() const noexcept;
	rg::ResourceViewHandle getResultRGViewHandle() const noexcept;
	EntityID getPickedEntity() const noexcept;
	void clearDebugGeometry() noexcept;
	void drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept;
	void drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept;

private:
	gal::GraphicsDevice *m_device = nullptr;
	ResourceViewRegistry *m_viewRegistry = nullptr;
	MeshManager *m_meshManager = nullptr;
	MaterialManager *m_materialManager = nullptr;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_frame = 0;
	uint32_t m_pickingPosX = -1;
	uint32_t m_pickingPosY = -1;
	uint64_t m_pickedEntity = 0;
	RendererResources *m_rendererResources = nullptr;
	RenderViewResources *m_renderViewResources = nullptr;
	rg::ResourceViewHandle m_resultImageViewHandle = {};
	float *m_haltonJitter = nullptr;
	CommonViewData m_viewData[2] = {};
	bool m_ignoreHistory = true;
	uint32_t m_framesSinceLastResize = 0;

	LightManager *m_lightManager = nullptr;
	ForwardModule *m_forwardModule = nullptr;
	PostProcessModule *m_postProcessModule = nullptr;
	GridPass *m_gridPass = nullptr;
};