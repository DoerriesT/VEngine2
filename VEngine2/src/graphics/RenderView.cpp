#include "RenderView.h"
#include "LightManager.h"
#include "pass/ForwardModule.h"
#include "pass/PostProcessModule.h"
#include "pass/GridPass.h"
#include "RenderViewResources.h"
#include "ResourceViewRegistry.h"
#include "gal/Initializers.h"
#include <glm/gtc/type_ptr.hpp>
#include "component/TransformComponent.h"
#include "component/MeshComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "component/CameraComponent.h"
#include "component/OutlineComponent.h"
#include "component/ParticipatingMediumComponent.h"
#include <glm/gtx/transform.hpp>
#include "MeshManager.h"
#include "MaterialManager.h"
#include "animation/Skeleton.h"
#include "RenderGraph.h"
#include "RendererResources.h"
#include "LinearGPUBufferAllocator.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/numeric.h>
#include <EASTL/sort.h>
#include <glm/gtx/quaternion.hpp>
#include "utility/Utility.h"
#include "graphics/Camera.h"
#include "ecs/ECS.h"

using namespace gal;

static constexpr size_t k_haltonSampleCount = 16;

RenderView::RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *rendererResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_meshManager(meshManager),
	m_materialManager(materialManager),
	m_rendererResources(rendererResources),
	m_width(width),
	m_height(height),
	m_haltonJitter(new float[k_haltonSampleCount * 2])
{
	for (size_t i = 0; i < k_haltonSampleCount; ++i)
	{
		m_haltonJitter[i * 2 + 0] = util::halton(i + 1, 2) * 2.0f - 1.0f;
		m_haltonJitter[i * 2 + 1] = util::halton(i + 1, 3) * 2.0f - 1.0f;
	}

	m_renderViewResources = new RenderViewResources(m_device, viewRegistry, m_width, m_height);
	m_lightManager = new LightManager(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_forwardModule = new ForwardModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_postProcessModule = new PostProcessModule(m_device, offsetBufferSetLayout, viewRegistry->getDescriptorSetLayout());
	m_gridPass = new GridPass(m_device, offsetBufferSetLayout);
}

RenderView::~RenderView()
{
	delete m_lightManager;
	delete m_forwardModule;
	delete m_postProcessModule;
	delete m_gridPass;
	delete m_renderViewResources;
}

void RenderView::render(const Data &data, rg::RenderGraph *graph) noexcept
{
	if (m_width == 0 || m_height == 0)
	{
		return;
	}

	const size_t resIdx = m_frame & 1;
	const size_t prevResIdx = (m_frame + 1) & 1;

	// read back picking data
	{
		auto *buffer = m_renderViewResources->m_pickingDataReadbackBuffers[resIdx];
		uint32_t *pickingReadbackData = nullptr;
		buffer->map((void **)&pickingReadbackData);
		{
			MemoryRange range{ 0, sizeof(uint32_t) * 4 };
			buffer->invalidate(1, &range);
			m_pickedEntity = pickingReadbackData[1]; // [0] is depth, [1] is the picked entity
		}
		buffer->unmap();
	}

	// result image
	rg::ResourceHandle resultImageHandle = graph->importImage(m_renderViewResources->m_resultImage, "Render View Result", m_renderViewResources->m_resultImageState);
	rg::ResourceViewHandle resultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Render View Result", resultImageHandle, graph));
	m_resultImageViewHandle = resultImageViewHandle;

	// exposure buffer
	rg::ResourceHandle exposureBufferHandle = graph->importBuffer(m_renderViewResources->m_exposureDataBuffer, "Exposure Buffer", m_renderViewResources->m_exposureDataBufferState);
	rg::ResourceViewHandle exposureBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::create("Exposure Buffer", exposureBufferHandle, sizeof(float) * 4, 0, sizeof(float)));

	// picking readback buffer
	rg::ResourceHandle pickingReadbackBufferHandle = graph->importBuffer(m_renderViewResources->m_pickingDataReadbackBuffers[resIdx], "Picking Readback Buffer", &m_renderViewResources->m_pickingDataReadbackBufferStates[resIdx]);
	rg::ResourceViewHandle pickingReadbackBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::createDefault("Picking Readback Buffer", pickingReadbackBufferHandle, graph));

	// picking buffer
	rg::ResourceHandle pickingBufferHandle = graph->createBuffer(rg::BufferDesc::create("Picking Buffer", sizeof(uint32_t) * 4, gal::BufferUsageFlags::RW_BYTE_BUFFER_BIT | gal::BufferUsageFlags::TRANSFER_SRC_BIT | gal::BufferUsageFlags::CLEAR_BIT));
	rg::ResourceViewHandle pickingBufferViewHandle = graph->createBufferView(rg::BufferViewDesc::createDefault("Picking Buffer", pickingBufferHandle, graph));

	// TAA result
	rg::ResourceHandle taaResultImageHandle = graph->importImage(m_renderViewResources->m_temporalAAImages[resIdx], "Temporal AA Result", &m_renderViewResources->m_temporalAAImageStates[resIdx]);
	rg::ResourceViewHandle taaResultImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Temporal AA Result", taaResultImageHandle, graph));

	// TAA history
	rg::ResourceHandle taaHistoryImageHandle = graph->importImage(m_renderViewResources->m_temporalAAImages[prevResIdx], "Temporal AA History", &m_renderViewResources->m_temporalAAImageStates[prevResIdx]);
	rg::ResourceViewHandle taaHistoryImageViewHandle = graph->createImageView(rg::ImageViewDesc::createDefault("Temporal AA History", taaHistoryImageHandle, graph));

	const size_t haltonIdx = m_frame % k_haltonSampleCount;
	glm::mat4 jitterMatrix = data.m_effectSettings.m_taaEnabled ? glm::translate(glm::vec3(m_haltonJitter[haltonIdx * 2 + 0] / m_width, m_haltonJitter[haltonIdx * 2 + 1] / m_height, 0.0f)) : glm::identity<glm::mat4>();


	auto &viewData = m_viewData[resIdx];
	viewData.m_viewMatrix = data.m_viewMatrix;
	viewData.m_projectionMatrix = data.m_projectionMatrix;
	viewData.m_invViewMatrix = glm::inverse(viewData.m_viewMatrix);
	viewData.m_invProjectionMatrix = glm::inverse(viewData.m_projectionMatrix);
	viewData.m_jitteredProjectionMatrix = jitterMatrix * viewData.m_projectionMatrix;
	viewData.m_invJitteredProjectionMatrix = glm::inverse(viewData.m_jitteredProjectionMatrix);
	viewData.m_viewProjectionMatrix = viewData.m_projectionMatrix * viewData.m_viewMatrix;
	viewData.m_invViewProjectionMatrix = glm::inverse(viewData.m_viewProjectionMatrix);
	viewData.m_jitteredViewProjectionMatrix = viewData.m_jitteredProjectionMatrix * viewData.m_viewMatrix;
	viewData.m_invJitteredViewProjectionMatrix = glm::inverse(viewData.m_jitteredViewProjectionMatrix);

	viewData.m_prevViewMatrix = m_viewData[prevResIdx].m_viewMatrix;
	viewData.m_prevInvViewMatrix = m_viewData[prevResIdx].m_invViewMatrix;
	viewData.m_prevProjectionMatrix = m_viewData[prevResIdx].m_projectionMatrix;
	viewData.m_prevInvProjectionMatrix = m_viewData[prevResIdx].m_invProjectionMatrix;
	viewData.m_prevJitteredProjectionMatrix = m_viewData[prevResIdx].m_jitteredProjectionMatrix;
	viewData.m_prevInvJitteredProjectionMatrix = m_viewData[prevResIdx].m_invJitteredProjectionMatrix;
	viewData.m_prevViewProjectionMatrix = m_viewData[prevResIdx].m_viewProjectionMatrix;
	viewData.m_prevInvViewProjectionMatrix = m_viewData[prevResIdx].m_invViewProjectionMatrix;
	viewData.m_prevJitteredViewProjectionMatrix = m_viewData[prevResIdx].m_jitteredViewProjectionMatrix;
	viewData.m_prevInvJitteredViewProjectionMatrix = m_viewData[prevResIdx].m_invJitteredViewProjectionMatrix;

	viewData.m_viewMatrixDepthRow = glm::vec4(viewData.m_viewMatrix[0][2], viewData.m_viewMatrix[1][2], viewData.m_viewMatrix[2][2], viewData.m_viewMatrix[3][2]);
	viewData.m_cameraPosition = data.m_cameraPosition;
	viewData.m_near = data.m_nearPlane;
	viewData.m_far = data.m_farPlane;
	viewData.m_fovy = data.m_fovy;
	viewData.m_width = m_width;
	viewData.m_height = m_height;
	viewData.m_pickingPosX = m_pickingPosX;
	viewData.m_pickingPosY = m_pickingPosY;
	viewData.m_aspectRatio = m_width / static_cast<float>(m_height);
	viewData.m_frame = m_frame;
	viewData.m_time = data.m_time;
	viewData.m_deltaTime = data.m_deltaTime;
	viewData.m_device = m_device;
	viewData.m_rendererResources = m_rendererResources;
	viewData.m_viewResources = m_renderViewResources;
	viewData.m_viewRegistry = m_viewRegistry;
	viewData.m_resIdx = static_cast<uint32_t>(resIdx);
	viewData.m_prevResIdx = static_cast<uint32_t>(prevResIdx);
	viewData.m_gpuProfilingCtx = m_device->getProfilingContext();
	viewData.m_constantBufferAllocator = m_rendererResources->m_constantBufferLinearAllocators[data.m_renderResourceIdx];
	viewData.m_shaderResourceAllocator = m_rendererResources->m_shaderResourceBufferLinearAllocators[data.m_renderResourceIdx];
	viewData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferLinearAllocators[data.m_renderResourceIdx];
	viewData.m_offsetBufferSet = m_rendererResources->m_offsetBufferDescriptorSets[data.m_renderResourceIdx];
	viewData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	viewData.m_pickingBufferHandle = pickingBufferViewHandle;
	viewData.m_exposureBufferHandle = exposureBufferViewHandle;
	viewData.m_reflectionProbeArrayTextureViewHandle = data.m_reflectionProbeCacheTextureViewHandle;

	// prepare data
	const bool initializeExposureBuffer = m_frame < 2;
	eastl::fixed_vector<rg::ResourceUsageDesc, 8> prepareFrameDataPassUsages;
	prepareFrameDataPassUsages.push_back({ pickingBufferViewHandle, {gal::ResourceState::CLEAR_RESOURCE} });
	if (initializeExposureBuffer)
	{
		prepareFrameDataPassUsages.push_back({ exposureBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} });
	}
	graph->addPass("Prepare Frame Data", rg::QueueType::GRAPHICS, prepareFrameDataPassUsages.size(), prepareFrameDataPassUsages.data(), [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			// clear picking buffer
			cmdList->fillBuffer(registry.getBuffer(pickingBufferViewHandle), 0, sizeof(uint32_t) * 4, 0);

			// initialize exposure buffer
			if (initializeExposureBuffer)
			{
				float data[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmdList->updateBuffer(registry.getBuffer(exposureBufferViewHandle), 0, sizeof(data), data);
			}
		});

	m_lightManager->update(viewData, data.m_ecs, graph);
	m_lightManager->recordLightTileAssignment(graph, viewData);

	LightManager::ShadowRecordData shadowRecordData{};
	shadowRecordData.m_transformBufferHandle = data.m_transformsBufferViewHandle;
	shadowRecordData.m_skinningMatrixBufferHandle = data.m_skinningMatricesBufferViewHandle;
	shadowRecordData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	shadowRecordData.m_renderList = data.m_renderList;
	shadowRecordData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	shadowRecordData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

	m_lightManager->recordShadows(graph, viewData, shadowRecordData);


	ForwardModule::ResultData forwardModuleResultData;
	ForwardModule::Data forwardModuleData{};
	forwardModuleData.m_viewData = &viewData;
	forwardModuleData.m_transformBufferHandle = data.m_transformsBufferViewHandle;
	forwardModuleData.m_prevTransformBufferHandle = data.m_prevTransformsBufferViewHandle;
	forwardModuleData.m_skinningMatrixBufferHandle = data.m_skinningMatricesBufferViewHandle;
	forwardModuleData.m_prevSkinningMatrixBufferHandle = data.m_prevSkinningMatricesBufferViewHandle;
	forwardModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	forwardModuleData.m_globalMediaBufferHandle = data.m_globalMediaBufferViewHandle;
	forwardModuleData.m_reflectionProbeDataBufferHandle = data.m_reflectionProbeDataBufferHandle;
	forwardModuleData.m_globalMediaCount = data.m_globalMediaCount;
	forwardModuleData.m_reflectionProbeCount = data.m_reflectionProbeCount;
	forwardModuleData.m_renderList = data.m_renderList;
	forwardModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	forwardModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	forwardModuleData.m_lightRecordData = m_lightManager->getLightRecordData();
	forwardModuleData.m_taaEnabled = data.m_effectSettings.m_taaEnabled;
	forwardModuleData.m_ignoreHistory = m_framesSinceLastResize < 2 || m_ignoreHistory;

	m_forwardModule->record(graph, forwardModuleData, &forwardModuleResultData);


	PostProcessModule::Data postProcessModuleData{};
	postProcessModuleData.m_viewData = &viewData;
	postProcessModuleData.m_lightingImageView = forwardModuleResultData.m_lightingImageViewHandle;
	postProcessModuleData.m_depthBufferImageViewHandle = forwardModuleResultData.m_depthBufferImageViewHandle;
	postProcessModuleData.m_velocityImageViewHandle = forwardModuleResultData.m_velocityImageViewHandle;
	postProcessModuleData.m_temporalAAResultImageViewHandle = taaResultImageViewHandle;
	postProcessModuleData.m_temporalAAHistoryImageViewHandle = taaHistoryImageViewHandle;
	postProcessModuleData.m_resultImageViewHandle = resultImageViewHandle;
	postProcessModuleData.m_transformBufferHandle = data.m_transformsBufferViewHandle;
	postProcessModuleData.m_skinningMatrixBufferHandle = data.m_skinningMatricesBufferViewHandle;
	postProcessModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	postProcessModuleData.m_renderList = data.m_renderList;
	postProcessModuleData.m_outlineRenderList = data.m_outlineRenderList;
	postProcessModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	postProcessModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	postProcessModuleData.m_debugNormals = data.m_effectSettings.m_renderDebugNormals;
	postProcessModuleData.m_renderOutlines = data.m_outlineRenderList && 
		(!data.m_outlineRenderList->m_opaque.empty()
		|| !data.m_outlineRenderList->m_opaqueAlphaTested.empty() 
		|| !data.m_outlineRenderList->m_opaqueSkinned.empty() 
		|| !data.m_outlineRenderList->m_opaqueSkinnedAlphaTested.empty());
	postProcessModuleData.m_ignoreHistory = m_framesSinceLastResize < 2 || m_ignoreHistory;
	postProcessModuleData.m_taaEnabled = data.m_effectSettings.m_taaEnabled;
	postProcessModuleData.m_sharpenEnabled = data.m_effectSettings.m_sharpenEnabled;
	postProcessModuleData.m_bloomEnabled = data.m_effectSettings.m_bloomEnabled;

	m_postProcessModule->record(graph, postProcessModuleData, nullptr);


	if (data.m_effectSettings.m_renderEditorGrid)
	{
		GridPass::Data gridPassData{};
		gridPassData.m_profilingCtx = m_device->getProfilingContext();
		gridPassData.m_bufferAllocator = viewData.m_constantBufferAllocator;
		gridPassData.m_offsetBufferSet = viewData.m_offsetBufferSet;
		gridPassData.m_width = m_width;
		gridPassData.m_height = m_height;
		gridPassData.m_colorAttachment = resultImageViewHandle;
		gridPassData.m_depthBufferAttachment = forwardModuleResultData.m_depthBufferImageViewHandle;
		gridPassData.m_modelMatrix = glm::translate(glm::floor(glm::vec3(viewData.m_cameraPosition.x, 0.0f, viewData.m_cameraPosition.z) * 0.1f) * 10.0f);
		gridPassData.m_viewProjectionMatrix = viewData.m_viewProjectionMatrix;
		gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
		gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
		gridPassData.m_cameraPos = viewData.m_cameraPosition;
		gridPassData.m_cellSize = 1.0f;
		gridPassData.m_gridSize = 200.0f;

		m_gridPass->record(graph, gridPassData);
	}

	// copy picking data to readback buffer
	rg::ResourceUsageDesc pickingBufferReadbackUsages[]
	{
		{ pickingBufferViewHandle, {gal::ResourceState::READ_TRANSFER} },
		{ pickingReadbackBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} },
	};
	graph->addPass("Readback Picking Buffer", rg::QueueType::GRAPHICS, eastl::size(pickingBufferReadbackUsages), pickingBufferReadbackUsages, [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			BufferCopy bufferCopy{};
			bufferCopy.m_srcOffset = 0;
			bufferCopy.m_dstOffset = 0;
			bufferCopy.m_size = sizeof(uint32_t) * 4;
			cmdList->copyBuffer(registry.getBuffer(pickingBufferViewHandle), registry.getBuffer(pickingReadbackBufferViewHandle), 1, &bufferCopy);
		});

	++m_frame;
	++m_framesSinceLastResize;
	m_ignoreHistory = false;
}

void RenderView::resize(uint32_t width, uint32_t height) noexcept
{
	if (width != 0 && height != 0)
	{
		m_width = width;
		m_height = height;
		m_renderViewResources->resize(width, height);
		m_ignoreHistory = true;
		m_framesSinceLastResize = 0;
	}
}

void RenderView::setPickingPos(uint32_t x, uint32_t y) noexcept
{
	m_pickingPosX = x;
	m_pickingPosY = y;
}

gal::Image *RenderView::getResultImage() const noexcept
{
	return m_renderViewResources->m_resultImage;
}

gal::ImageView *RenderView::getResultImageView() const noexcept
{
	return m_renderViewResources->m_resultImageView;
}

TextureViewHandle RenderView::getResultTextureViewHandle() const noexcept
{
	return m_renderViewResources->m_resultImageTextureViewHandle;
}

rg::ResourceViewHandle RenderView::getResultRGViewHandle() const noexcept
{
	return m_resultImageViewHandle;
}

EntityID RenderView::getPickedEntity() const noexcept
{
	return m_pickedEntity;
}

void RenderView::clearDebugGeometry() noexcept
{
	m_postProcessModule->clearDebugGeometry();
}

void RenderView::drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept
{
	m_postProcessModule->drawDebugLine(visibility, position0, position1, color0, color1);
}

void RenderView::drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept
{
	m_postProcessModule->drawDebugTriangle(visibility, position0, position1, position2, color0, color1, color2);
}
