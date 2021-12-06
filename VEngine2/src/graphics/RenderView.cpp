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
#include "component/LightComponent.h"
#include "component/CameraComponent.h"
#include "component/OutlineComponent.h"
#include <glm/gtx/transform.hpp>
#include "MeshManager.h"
#include "MaterialManager.h"
#include "animation/Skeleton.h"
#include "RenderGraph.h"
#include "RendererResources.h"
#include "BufferStackAllocator.h"
#include "RenderWorld.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/numeric.h>
#include <EASTL/sort.h>
#include <glm/gtx/quaternion.hpp>
#include "CommonViewData.h"

using namespace gal;

RenderView::RenderView(gal::GraphicsDevice *device, ResourceViewRegistry *viewRegistry, MeshManager *meshManager, MaterialManager *materialManager, RendererResources *rendererResources, gal::DescriptorSetLayout *offsetBufferSetLayout, uint32_t width, uint32_t height) noexcept
	:m_device(device),
	m_viewRegistry(viewRegistry),
	m_meshManager(meshManager),
	m_materialManager(materialManager),
	m_rendererResources(rendererResources),
	m_width(width),
	m_height(height)
{
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

void RenderView::render(
	float deltaTime,
	float time,
	const RenderWorld &renderWorld,
	rg::RenderGraph *graph,
	BufferStackAllocator *bufferAllocator,
	gal::DescriptorSet *offsetBufferSet,
	const float *viewMatrix,
	const float *projectionMatrix,
	const float *cameraPosition,
	const CameraComponent *cameraComponent,
	uint32_t pickingPosX,
	uint32_t pickingPosY
) noexcept
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

	CommonViewData viewData{};
	viewData.m_viewMatrix = glm::make_mat4(viewMatrix);
	viewData.m_invViewMatrix = glm::inverse(viewData.m_viewMatrix);
	viewData.m_projectionMatrix = glm::make_mat4(projectionMatrix);
	viewData.m_invProjectionMatrix = glm::inverse(viewData.m_projectionMatrix);
	viewData.m_viewProjectionMatrix = viewData.m_projectionMatrix * viewData.m_viewMatrix;
	viewData.m_invViewProjectionMatrix = glm::inverse(viewData.m_viewProjectionMatrix);
	viewData.m_viewMatrixDepthRow = glm::vec4(viewData.m_viewMatrix[0][2], viewData.m_viewMatrix[1][2], viewData.m_viewMatrix[2][2], viewData.m_viewMatrix[3][2]);
	viewData.m_cameraPosition = glm::make_vec3(cameraPosition);
	viewData.m_near = cameraComponent->m_near;
	viewData.m_far = cameraComponent->m_far;
	viewData.m_fovy = cameraComponent->m_fovy;
	viewData.m_width = m_width;
	viewData.m_height = m_height;
	viewData.m_aspectRatio = cameraComponent->m_aspectRatio;
	viewData.m_frame = m_frame;
	viewData.m_device = m_device;
	viewData.m_rendererResources = m_rendererResources;
	viewData.m_viewRegistry = m_viewRegistry;
	viewData.m_resIdx = static_cast<uint32_t>(resIdx);
	viewData.m_prevResIdx = static_cast<uint32_t>(prevResIdx);

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

	m_modelMatrices.clear();
	m_globalMedia.clear();
	m_renderList.clear();
	m_outlineRenderList.clear();

	const glm::mat4 invViewMatrix = glm::inverse(glm::make_mat4(viewMatrix));

	// process global participating media
	for (const auto &pm : renderWorld.m_globalParticipatingMedia)
	{
		GlobalParticipatingMediumGPU medium{};
		medium.emissive = glm::unpackUnorm4x8(pm.m_emissiveColor) * pm.m_emissiveIntensity;
		medium.extinction = pm.m_extinction;
		medium.scattering = glm::unpackUnorm4x8(pm.m_albedo) * pm.m_extinction;
		medium.phase = pm.m_phaseAnisotropy;
		medium.heightFogEnabled = pm.m_heightFogEnabled;
		medium.heightFogStart = pm.m_heightFogStart;
		medium.heightFogFalloff = pm.m_heightFogFalloff;
		medium.maxHeight = pm.m_maxHeight;
		medium.textureScale = pm.m_textureScale;
		medium.textureBias = glm::make_vec3(pm.m_textureBias);
		medium.densityTexture = pm.m_densityTextureHandle;

		m_globalMedia.push_back(medium);
	}

	// create transform matrices
	for (size_t i = 0; i < renderWorld.m_meshTransformsCount; ++i)
	{
		const auto &transform = renderWorld.m_interpolatedTransforms[i + renderWorld.m_meshTransformsOffset];
		glm::mat4 modelMatrix = glm::translate(transform.m_translation) * glm::mat4_cast(transform.m_rotation) * glm::scale(transform.m_scale);
		m_modelMatrices.push_back(modelMatrix);
	}

	// fill mesh render lists
	bool anyOutlines = false;
	for (const auto &mesh : renderWorld.m_meshes)
	{
		SubMeshInstanceData instanceData{};
		instanceData.m_subMeshHandle = mesh.m_subMeshHandle;
		instanceData.m_transformIndex = static_cast<uint32_t>(mesh.m_transformIndex - renderWorld.m_meshTransformsOffset);
		instanceData.m_skinningMatricesOffset = static_cast<uint32_t>(mesh.m_skinningMatricesOffset);
		instanceData.m_materialHandle = mesh.m_materialHandle;
		instanceData.m_entityID = mesh.m_entity;

		const bool alphaTested = m_materialManager->getMaterial(instanceData.m_materialHandle).m_alphaMode == MaterialCreateInfo::Alpha::Mask;
		const bool skinned = mesh.m_skinningMatricesCount != 0;

		if (alphaTested)
			if (skinned)
				m_renderList.m_opaqueSkinnedAlphaTested.push_back(instanceData);
			else
				m_renderList.m_opaqueAlphaTested.push_back(instanceData);
		else
			if (skinned)
				m_renderList.m_opaqueSkinned.push_back(instanceData);
			else
				m_renderList.m_opaque.push_back(instanceData);

		if (mesh.m_outlined)
		{
			anyOutlines = true;
			if (alphaTested)
				if (skinned)
					m_outlineRenderList.m_opaqueSkinnedAlphaTested.push_back(instanceData);
				else
					m_outlineRenderList.m_opaqueAlphaTested.push_back(instanceData);
			else
				if (skinned)
					m_outlineRenderList.m_opaqueSkinned.push_back(instanceData);
				else
					m_outlineRenderList.m_opaque.push_back(instanceData);
		}
	}

	auto createShaderResourceBuffer = [&](DescriptorType descriptorType, auto dataAsVector)
	{
		const size_t elementSize = sizeof(dataAsVector[0]);

		// prepare DescriptorBufferInfo
		DescriptorBufferInfo bufferInfo = Initializers::structuedBufferInfo(elementSize, dataAsVector.size());
		bufferInfo.m_buffer = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->getBuffer();

		// allocate memory
		uint64_t alignment = m_device->getBufferAlignment(descriptorType, elementSize);
		uint8_t *bufferPtr = m_rendererResources->m_shaderResourceBufferStackAllocators[resIdx]->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

		// copy to destination
		memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() * elementSize);

		// create a transient bindless handle
		return descriptorType == DescriptorType::STRUCTURED_BUFFER ?
			m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true) : 
			m_viewRegistry->createByteBufferViewHandle(bufferInfo, true);
	};

	StructuredBufferViewHandle skinningMatricesBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, renderWorld.m_skinningMatrices);

	StructuredBufferViewHandle globalMediaBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(DescriptorType::STRUCTURED_BUFFER, m_globalMedia);

	// prepare data
	eastl::fixed_vector<rg::ResourceUsageDesc, 8> prepareFrameDataPassUsages;
	prepareFrameDataPassUsages.push_back({ pickingBufferViewHandle, {gal::ResourceState::CLEAR_RESOURCE} });
	if (m_frame == 0)
	{
		prepareFrameDataPassUsages.push_back({ exposureBufferViewHandle, {gal::ResourceState::WRITE_TRANSFER} });
	}
	graph->addPass("Prepare Frame Data", rg::QueueType::GRAPHICS, prepareFrameDataPassUsages.size(), prepareFrameDataPassUsages.data(), [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			// clear picking buffer
			cmdList->fillBuffer(registry.getBuffer(pickingBufferViewHandle), 0, sizeof(uint32_t) * 4, 0);

			// initialize exposure buffer
			if (m_frame == 0)
			{
				float data[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				cmdList->updateBuffer(registry.getBuffer(exposureBufferViewHandle), 0, sizeof(data), data);
			}
		});

	m_lightManager->update(viewData, renderWorld, graph);
	m_lightManager->recordLightTileAssignment(graph, viewData);

	LightManager::ShadowRecordData shadowRecordData{};
	shadowRecordData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	shadowRecordData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	shadowRecordData.m_renderList = &m_renderList;
	shadowRecordData.m_modelMatrices = m_modelMatrices.data();
	shadowRecordData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	shadowRecordData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

	m_lightManager->recordShadows(graph, viewData, shadowRecordData);


	ForwardModule::ResultData forwardModuleResultData;
	ForwardModule::Data forwardModuleData{};
	forwardModuleData.m_profilingCtx = m_device->getProfilingContext();
	forwardModuleData.m_bufferAllocator = bufferAllocator;
	forwardModuleData.m_offsetBufferSet = offsetBufferSet;
	forwardModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	forwardModuleData.m_width = m_width;
	forwardModuleData.m_height = m_height;
	forwardModuleData.m_frame = m_frame;
	forwardModuleData.m_fovy = cameraComponent->m_fovy;
	forwardModuleData.m_nearPlane = cameraComponent->m_near;
	forwardModuleData.m_pickingPosX = pickingPosX;
	forwardModuleData.m_pickingPosY = pickingPosY;
	forwardModuleData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	forwardModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	forwardModuleData.m_globalMediaBufferHandle = globalMediaBufferViewHandle;
	forwardModuleData.m_pickingBufferHandle = pickingBufferViewHandle;
	forwardModuleData.m_exposureBufferHandle = exposureBufferViewHandle;
	forwardModuleData.m_globalMediaCount = static_cast<uint32_t>(m_globalMedia.size());
	forwardModuleData.m_viewProjectionMatrix = viewData.m_viewProjectionMatrix;
	forwardModuleData.m_invViewProjectionMatrix = viewData.m_invViewProjectionMatrix;
	forwardModuleData.m_viewMatrix = viewData.m_viewMatrix;
	forwardModuleData.m_invProjectionMatrix = viewData.m_invProjectionMatrix;
	forwardModuleData.m_cameraPosition = viewData.m_cameraPosition;
	forwardModuleData.m_renderList = &m_renderList;
	forwardModuleData.m_modelMatrices = m_modelMatrices.data();
	forwardModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	forwardModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	forwardModuleData.m_rendererResources = m_rendererResources;
	forwardModuleData.m_lightRecordData = m_lightManager->getLightRecordData();

	m_forwardModule->record(graph, forwardModuleData, &forwardModuleResultData);


	PostProcessModule::Data postProcessModuleData{};
	postProcessModuleData.m_profilingCtx = m_device->getProfilingContext();
	postProcessModuleData.m_bufferAllocator = bufferAllocator;
	postProcessModuleData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferStackAllocators[resIdx];
	postProcessModuleData.m_offsetBufferSet = offsetBufferSet;
	postProcessModuleData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
	postProcessModuleData.m_width = m_width;
	postProcessModuleData.m_height = m_height;
	postProcessModuleData.m_deltaTime = deltaTime;
	postProcessModuleData.m_time = time;
	postProcessModuleData.m_lightingImageView = forwardModuleResultData.m_lightingImageViewHandle;
	postProcessModuleData.m_depthBufferImageViewHandle = forwardModuleResultData.m_depthBufferImageViewHandle;
	postProcessModuleData.m_resultImageViewHandle = resultImageViewHandle;
	postProcessModuleData.m_exposureBufferViewHandle = exposureBufferViewHandle;
	postProcessModuleData.m_skinningMatrixBufferHandle = skinningMatricesBufferViewHandle;
	postProcessModuleData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
	postProcessModuleData.m_viewProjectionMatrix = viewData.m_viewProjectionMatrix;
	postProcessModuleData.m_cameraPosition = viewData.m_cameraPosition;
	postProcessModuleData.m_renderList = &m_renderList;
	postProcessModuleData.m_outlineRenderList = &m_outlineRenderList;
	postProcessModuleData.m_modelMatrices = m_modelMatrices.data();
	postProcessModuleData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
	postProcessModuleData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
	postProcessModuleData.m_debugNormals = false;
	postProcessModuleData.m_renderOutlines = anyOutlines;

	m_postProcessModule->record(graph, postProcessModuleData, nullptr);


	GridPass::Data gridPassData{};
	gridPassData.m_profilingCtx = m_device->getProfilingContext();
	gridPassData.m_bufferAllocator = bufferAllocator;
	gridPassData.m_offsetBufferSet = offsetBufferSet;
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
}

void RenderView::resize(uint32_t width, uint32_t height) noexcept
{
	if (width != 0 && height != 0)
	{
		m_width = width;
		m_height = height;
		m_renderViewResources->resize(width, height);
	}
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
