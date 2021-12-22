#pragma once
#include <EASTL/vector.h>
#include <glm/mat4x4.hpp>
#include "utility/DeletedCopyMove.h"
#include "RenderData.h"
#include "RenderGraph.h"

struct RenderWorld;
struct CommonViewData;
class RenderGraph;
struct SubMeshDrawInfo;
struct RenderList;
struct SubMeshBufferHandles;
class ECS;
struct TransformComponent;
struct LightComponent;

struct LightRecordData
{
	uint32_t m_directionalLightCount;
	uint32_t m_directionalLightShadowedCount;
	uint32_t m_punctualLightCount;
	uint32_t m_punctualLightShadowedCount;
	uint32_t m_shadowMapViewHandleCount;
	StructuredBufferViewHandle m_directionalLightsBufferViewHandle;
	StructuredBufferViewHandle m_directionalLightsShadowedBufferViewHandle;
	StructuredBufferViewHandle m_punctualLightsBufferViewHandle;
	rg::ResourceViewHandle m_punctualLightsTileTextureViewHandle;
	ByteBufferViewHandle m_punctualLightsDepthBinsBufferViewHandle;
	StructuredBufferViewHandle m_punctualLightsShadowedBufferViewHandle;
	rg::ResourceViewHandle m_punctualLightsShadowedTileTextureViewHandle;
	ByteBufferViewHandle m_punctualLightsShadowedDepthBinsBufferViewHandle;
	const rg::ResourceViewHandle *m_shadowMapViewHandles;
};

class LightManager
{
public:
	struct ShadowRecordData
	{
		StructuredBufferViewHandle m_transformBufferHandle;
		StructuredBufferViewHandle m_skinningMatrixBufferHandle;
		StructuredBufferViewHandle m_materialsBufferHandle;
		const RenderList *m_renderList;
		const glm::mat4 *m_modelMatrices;
		const SubMeshDrawInfo *m_meshDrawInfo;
		const SubMeshBufferHandles *m_meshBufferHandles;
	};


	explicit LightManager(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(LightManager);
	~LightManager();
	void update(const CommonViewData &viewData, ECS *ecs, uint64_t cameraEntity, rg::RenderGraph *graph) noexcept;
	void recordLightTileAssignment(rg::RenderGraph *graph, const CommonViewData &viewData) noexcept;
	void recordShadows(rg::RenderGraph *graph, const CommonViewData &viewData, const ShadowRecordData &data) noexcept;
	const LightRecordData *getLightRecordData() const noexcept;

private:
	struct LightPointers
	{
		TransformComponent *m_tc;
		LightComponent *m_lc;
	};

	gal::GraphicsDevice *m_device;
	eastl::vector<glm::mat4> m_shadowMatrices;
	eastl::vector<rg::ResourceViewHandle> m_shadowTextureRenderHandles;
	eastl::vector<rg::ResourceViewHandle> m_shadowTextureSampleHandles;
	eastl::vector<DirectionalLightGPU> m_directionalLights;
	eastl::vector<DirectionalLightGPU> m_shadowedDirectionalLights;
	eastl::vector<PunctualLightGPU> m_punctualLights;
	eastl::vector<PunctualLightShadowedGPU> m_punctualLightsShadowed;
	eastl::vector<uint32_t> m_punctualLightsDepthBins;
	eastl::vector<uint32_t> m_punctualLightsShadowedDepthBins;
	eastl::vector<glm::mat4> m_lightTransforms;

	LightRecordData m_lightRecordData = {};
	StructuredBufferViewHandle m_lightTransformsBufferViewHandle = {};

	gal::GraphicsPipeline *m_lightTileAssignmentPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowAlphaTestedPipeline = nullptr;
	gal::GraphicsPipeline *m_shadowSkinnedAlphaTestedPipeline = nullptr;

	void processDirectionalLights(const CommonViewData &viewData, rg::RenderGraph *graph, const eastl::vector<LightPointers> &lightPtrs) noexcept;
	void processShadowedDirectionalLights(const CommonViewData &viewData, rg::RenderGraph *graph, const eastl::vector<LightPointers> &lightPtrs) noexcept;
	void processPunctualLights(const CommonViewData &viewData, rg::RenderGraph *graph, const eastl::vector<LightPointers> &lightPtrs) noexcept;
	void processShadowedPunctualLights(const CommonViewData &viewData, rg::RenderGraph *graph, const eastl::vector<LightPointers> &lightPtrs) noexcept;
};