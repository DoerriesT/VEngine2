#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "LinearGPUBufferAllocator.h"
#include "ResourceViewRegistry.h"
#include "RendererResources.h"
#include "ReflectionProbeManager.h"
#include "IrradianceVolumeManager.h"
#include "RenderView.h"
#include "pass/ImGuiPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include "imgui/imgui.h"
#include "MeshManager.h"
#include "TextureLoader.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "component/TransformComponent.h"
#include "component/CameraComponent.h"
#include "component/MeshComponent.h"
#include "component/OutlineComponent.h"
#include "component/ParticipatingMediumComponent.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "RenderGraph.h"
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/SkinnedMeshComponent.h"
#include "Camera.h"
#include "MeshRenderWorld.h"

bool g_taaEnabled = true;
bool g_sharpenEnabled = true;

static Transform lerp(const Transform &x, const Transform &y, float alpha) noexcept
{
	Transform result;
	result.m_translation = glm::mix(x.m_translation, y.m_translation, alpha);
	result.m_rotation = glm::normalize(glm::slerp(x.m_rotation, y.m_rotation, alpha));
	result.m_scale = glm::mix(x.m_scale, y.m_scale, alpha);
	return result;
}

static void lerp(size_t count, const glm::mat4 *x, const glm::mat4 *y, float alpha, glm::mat4 *result) noexcept
{
	for (size_t i = 0; i < count; ++i)
	{
		for (size_t j = 0; j < 16; ++j)
		{
			(&result[i][0][0])[j] = glm::mix((&x[i][0][0])[j], (&y[i][0][0])[j], alpha);
		}
	}
}

Renderer::Renderer(void *windowHandle, uint32_t width, uint32_t height) noexcept
{
	m_swapchainWidth = width;
	m_swapchainHeight = height;
	m_width = width;
	m_height = height;

	m_device = gal::GraphicsDevice::create(windowHandle, false, gal::GraphicsBackendType::D3D12);
	m_device->createSwapChain(m_device->getGraphicsQueue(), m_swapchainWidth, m_swapchainHeight, false, gal::PresentMode::V_SYNC, &m_swapChain);

	m_device->createSemaphore(0, &m_semaphores[0]);
	m_device->createSemaphore(0, &m_semaphores[1]);
	m_device->createSemaphore(0, &m_semaphores[2]);

	m_device->setDebugObjectName(gal::ObjectType::SEMAPHORE, m_semaphores[0], "Graphics Queue Semaphore");
	m_device->setDebugObjectName(gal::ObjectType::SEMAPHORE, m_semaphores[1], "Compute Queue Semaphore");
	m_device->setDebugObjectName(gal::ObjectType::SEMAPHORE, m_semaphores[2], "Transfer Queue Semaphore");

	m_viewRegistry = new ResourceViewRegistry(m_device);
	m_meshManager = new MeshManager(m_device, m_viewRegistry);
	m_textureLoader = new TextureLoader(m_device);
	m_textureManager = new TextureManager(m_device, m_viewRegistry);
	m_rendererResources = new RendererResources(m_device, m_viewRegistry, m_textureLoader);
	m_materialManager = new MaterialManager(m_device, m_textureManager, m_rendererResources->m_materialsBuffer);

	m_renderGraph = new rg::RenderGraph(m_device, m_semaphores, m_semaphoreValues, m_viewRegistry);

	m_reflectionProbeManager = new ReflectionProbeManager(m_device, m_rendererResources, m_viewRegistry);
	m_irradianceVolumeManager = new IrradianceVolumeManager(m_device, m_rendererResources, m_viewRegistry);

	m_renderView = new RenderView(m_device, m_viewRegistry, m_meshManager, m_materialManager, m_rendererResources, m_rendererResources->m_offsetBufferDescriptorSetLayout, width, height);

	m_imguiPass = new ImGuiPass(m_device, m_viewRegistry->getDescriptorSetLayout());

	m_meshRenderWorld = new MeshRenderWorld(m_device, m_viewRegistry, m_meshManager, m_materialManager);
}

Renderer::~Renderer() noexcept
{
	m_device->waitIdle();
	delete m_renderGraph;
	m_renderGraph = nullptr;

	delete m_meshRenderWorld;
	delete m_imguiPass;
	delete m_renderView;
	delete m_irradianceVolumeManager;
	delete m_reflectionProbeManager;
	delete m_materialManager;
	delete m_textureLoader;
	delete m_textureManager;
	delete m_meshManager;
	delete m_rendererResources;
	delete m_viewRegistry;

	m_device->destroySemaphore(m_semaphores[0]);
	m_device->destroySemaphore(m_semaphores[1]);
	m_device->destroySemaphore(m_semaphores[2]);
	m_device->destroySwapChain();

	gal::GraphicsDevice::destroy(m_device);
}

void Renderer::render(float deltaTime, ECS *ecs, uint64_t cameraEntity, float fractionalSimFrameTime) noexcept
{
	PROFILING_ZONE_SCOPED;

	// interpolate transforms
	{
		PROFILING_ZONE_SCOPED_N("Transform Interpolation");

		IterateQuery iterateQuery;
		ecs->setIterateQueryRequiredComponents<TransformComponent>(iterateQuery);
		ecs->setIterateQueryOptionalComponents<SkinnedMeshComponent>(iterateQuery);
		ecs->iterate<TransformComponent, SkinnedMeshComponent>(iterateQuery, [&](size_t count, const EntityID *entities, TransformComponent *transC, SkinnedMeshComponent *skinnedMeshC)
			{
				for (size_t i = 0; i < count; ++i)
				{
					auto &tc = transC[i];
					tc.m_prevRenderTransform = tc.m_curRenderTransform;
					tc.m_curRenderTransform = lerp(tc.m_prevTransform, tc.m_transform, fractionalSimFrameTime);

					if (skinnedMeshC)
					{
						auto &sc = skinnedMeshC[i];
						assert(sc.m_matrixPalette.size() == sc.m_skeleton->getSkeleton()->getJointCount());
						assert(sc.m_matrixPalette.size() == sc.m_prevMatrixPalette.size());
						assert(sc.m_matrixPalette.size() == sc.m_prevRenderMatrixPalette.size());

						bool hasPrevRenderPalette = !sc.m_curRenderMatrixPalette.empty();

						if (hasPrevRenderPalette)
						{
							sc.m_prevRenderMatrixPalette = eastl::move(sc.m_curRenderMatrixPalette);
						}

						// interpolate palette
						sc.m_curRenderMatrixPalette.resize(sc.m_matrixPalette.size());
						lerp(sc.m_matrixPalette.size(), sc.m_prevMatrixPalette.data(), sc.m_matrixPalette.data(), fractionalSimFrameTime, sc.m_curRenderMatrixPalette.data());

						if (!hasPrevRenderPalette)
						{
							sc.m_prevRenderMatrixPalette = sc.m_curRenderMatrixPalette;
						}
					}
				}
			});
	}


	m_renderGraph->nextFrame();
	m_time += deltaTime;

	PROFILING_GPU_NEW_FRAME(m_device->getProfilingContext());

	m_rendererResources->m_constantBufferLinearAllocators[m_frame & 1]->reset();
	m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1]->reset();
	m_rendererResources->m_indexBufferLinearAllocators[m_frame & 1]->reset();
	m_rendererResources->m_vertexBufferLinearAllocators[m_frame & 1]->reset();

	m_textureManager->flushDeletionQueue(m_frame);
	m_meshManager->flushDeletionQueue(m_frame);

	{
		PROFILING_ZONE_SCOPED_N("Record CommandList");

		// upload textures and meshes
		m_renderGraph->addPass("Flush Uploads", rg::QueueType::GRAPHICS, 0, nullptr, [=](gal::CommandList *cmdList, const rg::Registry &registry)
			{
				m_textureLoader->flushUploadCopies(cmdList, m_frame);
				m_meshManager->flushUploadCopies(cmdList, m_frame);
			});

		auto createShaderResourceBuffer = [&](gal::DescriptorType descriptorType, auto dataAsVector)
		{
			const size_t elementSize = sizeof(dataAsVector[0]);

			// prepare DescriptorBufferInfo
			gal::DescriptorBufferInfo bufferInfo = gal::Initializers::structuredBufferInfo(elementSize, dataAsVector.size());
			bufferInfo.m_buffer = m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1]->getBuffer();

			// allocate memory
			uint64_t alignment = m_device->getBufferAlignment(descriptorType, elementSize);
			uint8_t *bufferPtr = m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1]->allocate(alignment, &bufferInfo.m_range, &bufferInfo.m_offset);

			// copy to destination
			memcpy(bufferPtr, dataAsVector.data(), dataAsVector.size() * elementSize);

			// create a transient bindless handle
			return descriptorType == gal::DescriptorType::STRUCTURED_BUFFER ?
				m_viewRegistry->createStructuredBufferViewHandle(bufferInfo, true) :
				m_viewRegistry->createByteBufferViewHandle(bufferInfo, true);
		};

		m_globalMedia.clear();

		// process global participating media
		ecs->iterate<ParticipatingMediumComponent>([&](size_t count, const EntityID *entities, ParticipatingMediumComponent *mediaC)
			{
				for (size_t i = 0; i < count; ++i)
				{
					auto &mc = mediaC[i];

					if (mc.m_type == ParticipatingMediumComponent::Type::Global)
					{
						GlobalParticipatingMediumGPU medium{};
						medium.emissive = glm::unpackUnorm4x8(mc.m_emissiveColor) * mc.m_emissiveIntensity;
						medium.extinction = mc.m_extinction;
						medium.scattering = glm::unpackUnorm4x8(mc.m_albedo) * mc.m_extinction;
						medium.phase = mc.m_phaseAnisotropy;
						medium.heightFogEnabled = mc.m_heightFogEnabled;
						medium.heightFogStart = mc.m_heightFogStart;
						medium.heightFogFalloff = mc.m_heightFogFalloff;
						medium.maxHeight = mc.m_maxHeight;
						medium.textureScale = mc.m_textureScale;
						medium.textureBias = glm::make_vec3(mc.m_textureBias);
						medium.densityTexture = mc.m_densityTexture.isLoaded() ? mc.m_densityTexture->getTextureHandle() : TextureHandle{};

						m_globalMedia.push_back(medium);
					}
				}
			});

		StructuredBufferViewHandle globalMediaBufferViewHandle = (StructuredBufferViewHandle)createShaderResourceBuffer(gal::DescriptorType::STRUCTURED_BUFFER, m_globalMedia);

		m_meshRenderWorld->update(ecs, m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1]);

		// render views
		if (cameraEntity != k_nullEntity)
		{
			auto *cameraComponent = ecs->getComponent<CameraComponent>(cameraEntity);
			auto cameraTransformComponent = *ecs->getComponent<TransformComponent>(cameraEntity);
			cameraTransformComponent.m_transform = cameraTransformComponent.m_curRenderTransform;
			Camera camera(cameraTransformComponent, *cameraComponent);

			// bake irradiance volume
			{
				IrradianceVolumeManager::Data irradianceVolumeMgrData{};
				irradianceVolumeMgrData.m_ecs = ecs;
				irradianceVolumeMgrData.m_gpuProfilingCtx = m_device->getProfilingContext();
				irradianceVolumeMgrData.m_shaderResourceLinearAllocator = m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1];
				irradianceVolumeMgrData.m_constantBufferLinearAllocator = m_rendererResources->m_constantBufferLinearAllocators[m_frame & 1];
				irradianceVolumeMgrData.m_offsetBufferSet = m_rendererResources->m_offsetBufferDescriptorSets[m_frame & 1];
				irradianceVolumeMgrData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
				irradianceVolumeMgrData.m_meshRenderWorld = m_meshRenderWorld;
				irradianceVolumeMgrData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
				irradianceVolumeMgrData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();
				irradianceVolumeMgrData.m_frame = m_frame;

				m_irradianceVolumeManager->update(m_renderGraph, irradianceVolumeMgrData);
			}

			// update reflection probes
			{
				auto cameraPosition = camera.getPosition();

				ReflectionProbeManager::Data reflectionProbeManagerData{};
				reflectionProbeManagerData.m_ecs = ecs;
				reflectionProbeManagerData.m_cameraPosition = &cameraPosition;
				reflectionProbeManagerData.m_gpuProfilingCtx = m_device->getProfilingContext();
				reflectionProbeManagerData.m_shaderResourceLinearAllocator = m_rendererResources->m_shaderResourceBufferLinearAllocators[m_frame & 1];
				reflectionProbeManagerData.m_constantBufferLinearAllocator = m_rendererResources->m_constantBufferLinearAllocators[m_frame & 1];
				reflectionProbeManagerData.m_offsetBufferSet = m_rendererResources->m_offsetBufferDescriptorSets[m_frame & 1];
				reflectionProbeManagerData.m_materialsBufferHandle = m_rendererResources->m_materialsBufferViewHandle;
				reflectionProbeManagerData.m_irradianceVolumeCount = m_irradianceVolumeManager->getIrradianceVolumeCount();
				reflectionProbeManagerData.m_irradianceVolumeBufferViewHandle = m_irradianceVolumeManager->getIrradianceVolumeBufferViewHandle();
				reflectionProbeManagerData.m_meshRenderWorld = m_meshRenderWorld;
				reflectionProbeManagerData.m_meshDrawInfo = m_meshManager->getSubMeshDrawInfoTable();
				reflectionProbeManagerData.m_meshBufferHandles = m_meshManager->getSubMeshBufferHandleTable();

				m_reflectionProbeManager->update(m_renderGraph, reflectionProbeManagerData);
			}

			RenderView::Data renderViewData{};
			renderViewData.m_effectSettings.m_postProcessingEnabled = true;
			renderViewData.m_effectSettings.m_renderEditorGrid = m_renderGrid;
			renderViewData.m_effectSettings.m_renderDebugNormals = false;
			renderViewData.m_effectSettings.m_taaEnabled = g_taaEnabled;
			renderViewData.m_effectSettings.m_sharpenEnabled = g_sharpenEnabled;
			renderViewData.m_effectSettings.m_bloomEnabled = true;
			renderViewData.m_effectSettings.m_pickingEnabled = true;
			renderViewData.m_effectSettings.m_renderOutlines = true;
			renderViewData.m_effectSettings.m_autoExposureEnabled = true;
			renderViewData.m_effectSettings.m_manualExposure = 1.0f;
			renderViewData.m_ecs = ecs;
			renderViewData.m_deltaTime = deltaTime;
			renderViewData.m_time = m_time;
			renderViewData.m_reflectionProbeCacheTextureViewHandle = m_reflectionProbeManager->getReflectionProbeArrayTextureViewHandle();
			renderViewData.m_reflectionProbeDataBufferHandle = m_reflectionProbeManager->getReflectionProbeDataBufferhandle();
			renderViewData.m_reflectionProbeCount = m_reflectionProbeManager->getReflectionProbeCount();
			renderViewData.m_globalMediaBufferViewHandle = globalMediaBufferViewHandle;
			renderViewData.m_globalMediaCount = static_cast<uint32_t>(m_globalMedia.size());
			renderViewData.m_irradianceVolumeBufferViewHandle = m_irradianceVolumeManager->getIrradianceVolumeBufferViewHandle();
			renderViewData.m_irradianceVolumeCount = m_irradianceVolumeManager->getIrradianceVolumeCount();
			renderViewData.m_meshRenderWorld = m_meshRenderWorld;
			renderViewData.m_viewMatrix = camera.getViewMatrix();
			renderViewData.m_projectionMatrix = camera.getProjectionMatrix();
			renderViewData.m_cameraPosition = camera.getPosition();
			renderViewData.m_nearPlane = cameraComponent->m_near;
			renderViewData.m_farPlane = cameraComponent->m_far;
			renderViewData.m_fovy = cameraComponent->m_fovy;
			renderViewData.m_renderResourceIdx = m_frame & 1;
			renderViewData.m_prevRenderResourceIdx = (m_frame + 1) & 1;

			m_renderView->render(renderViewData, m_renderGraph);
		}

		if (m_swapchainWidth != 0 && m_swapchainHeight != 0)
		{
			auto swapchainIndex = m_swapChain->getCurrentImageIndex();
			rg::ResourceHandle swapchainImageHandle = m_renderGraph->importImage(m_swapChain->getImage(swapchainIndex), "Swapchain Image");
			rg::ResourceViewHandle swapchainViewHandle = m_renderGraph->createImageView(rg::ImageViewDesc::createDefault("Swapchain Image View", swapchainImageHandle, m_renderGraph));

			rg::ResourceViewHandle renderViewResultImageViewHandle = m_renderView->getResultRGViewHandle();

			// copy view to swap chain
			if (!m_editorMode)
			{
				rg::ResourceUsageDesc usageDescs[] =
				{
					{renderViewResultImageViewHandle, {gal::ResourceState::READ_TRANSFER}},
					{swapchainViewHandle, {gal::ResourceState::WRITE_TRANSFER}},
				};
				m_renderGraph->addPass("Copy To Swapchain", rg::QueueType::GRAPHICS, eastl::size(usageDescs), usageDescs, [=](gal::CommandList *cmdList, const rg::Registry &registry)
					{
						gal::ImageCopy imageCopy{};
						imageCopy.m_srcLayerCount = 1;
						imageCopy.m_dstLayerCount = 1;
						imageCopy.m_extent = { m_swapchainWidth, m_swapchainHeight, 1 };
						cmdList->copyImage(registry.getImage(renderViewResultImageViewHandle), registry.getImage(swapchainViewHandle), 1, &imageCopy);
					});
			}
			else
			{
				// have the view result image be transitioned to READ_RESOURCE
				rg::ResourceUsageDesc viewResultTransitionUsageDesc = { renderViewResultImageViewHandle, {gal::ResourceState::READ_RESOURCE, gal::PipelineStageFlags::PIXEL_SHADER_BIT} };
				m_renderGraph->addPass("View Image Transition", rg::QueueType::GRAPHICS, 1, &viewResultTransitionUsageDesc, [=](gal::CommandList *, const rg::Registry &) {});

				// imgui
				{
					ImGuiPass::Data imguiPassData{};
					imguiPassData.m_profilingCtx = m_device->getProfilingContext();
					imguiPassData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferLinearAllocators[m_frame & 1];
					imguiPassData.m_indexBufferAllocator = m_rendererResources->m_indexBufferLinearAllocators[m_frame & 1];
					imguiPassData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
					imguiPassData.m_width = m_swapchainWidth;
					imguiPassData.m_height = m_swapchainHeight;
					imguiPassData.m_renderTargetHandle = swapchainViewHandle;
					imguiPassData.m_clear = true;
					imguiPassData.m_imGuiDrawData = ImGui::GetDrawData();

					m_imguiPass->record(m_renderGraph, imguiPassData);
				}
			}

			// have the swapchain image be transitioned to PRESENT
			rg::ResourceUsageDesc presentTransitionUsageDesc = { swapchainViewHandle, {gal::ResourceState::PRESENT} };
			m_renderGraph->addPass("Present Transition", rg::QueueType::GRAPHICS, 1, &presentTransitionUsageDesc, [=](gal::CommandList *cmdList, const rg::Registry &)
				{
					PROFILING_GPU_COLLECT(m_device->getProfilingContext(), cmdList);
				});
		}
	}

	m_viewRegistry->flushChanges();
	m_renderGraph->execute();

	if (m_swapchainWidth != 0 && m_swapchainHeight != 0)
	{
		m_swapChain->present(m_semaphores[0], m_semaphoreValues[0], m_semaphores[0], m_semaphoreValues[0] + 1);
		++m_semaphoreValues[0];
	}

	m_viewRegistry->swapSets();

	++m_frame;
}

void Renderer::resize(uint32_t swapchainWidth, uint32_t swapchainHeight, uint32_t width, uint32_t height) noexcept
{
	m_swapchainWidth = swapchainWidth;
	m_swapchainHeight = swapchainHeight;
	m_width = width;
	m_height = height;

	bool isIdle = false;

	if (m_swapchainWidth != 0 && m_swapchainHeight != 0)
	{
		m_device->waitIdle();
		isIdle = true;
		m_swapChain->resize(m_swapchainWidth, m_swapchainHeight, false, gal::PresentMode::IMMEDIATE);
	}

	if (m_width != 0 && m_height != 0)
	{
		if (!isIdle)
		{
			m_device->waitIdle();
		}
		m_renderView->resize(m_width, m_height);
	}
}

void Renderer::getResolution(uint32_t *swapchainWidth, uint32_t *swapchainHeight, uint32_t *width, uint32_t *height) noexcept
{
	*swapchainWidth = m_swapchainWidth;
	*swapchainHeight = m_swapchainHeight;
	*width = m_width;
	*height = m_height;
}

void Renderer::createSubMeshes(uint32_t count, SubMeshCreateInfo *subMeshes, SubMeshHandle *handles) noexcept
{
	m_meshManager->createSubMeshes(count, subMeshes, handles);
}

void Renderer::destroySubMeshes(uint32_t count, SubMeshHandle *handles) noexcept
{
	m_meshManager->destroySubMeshes(count, handles, m_frame);
}

TextureHandle Renderer::loadTexture(size_t fileSize, const char *fileData, const char *textureName) noexcept
{
	gal::Image *image = nullptr;
	gal::ImageView *view = nullptr;
	if (!m_textureLoader->load(fileSize, fileData, textureName, &image, &view))
	{
		return TextureHandle();
	}

	return m_textureManager->add(image, view);
}

TextureHandle Renderer::loadRawRGBA8(size_t fileSize, const char *fileData, const char *textureName, uint32_t width, uint32_t height) noexcept
{
	gal::Image *image = nullptr;
	gal::ImageView *view = nullptr;
	if (!m_textureLoader->loadRawRGBA8(fileSize, fileData, textureName, width, height, &image, &view))
	{
		return TextureHandle();
	}

	return m_textureManager->add(image, view);
}

void Renderer::destroyTexture(TextureHandle handle) noexcept
{
	m_textureManager->free(handle, m_frame);
}

void Renderer::createMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept
{
	m_materialManager->createMaterials(count, materials, handles);
}

void Renderer::updateMaterials(uint32_t count, const MaterialCreateInfo *materials, MaterialHandle *handles) noexcept
{
	m_materialManager->updateMaterials(count, materials, handles);
}

void Renderer::destroyMaterials(uint32_t count, MaterialHandle *handles) noexcept
{
	m_materialManager->destroyMaterials(count, handles);
}

ImTextureID Renderer::getImGuiTextureID(TextureHandle handle) noexcept
{
	return (ImTextureID)(size_t)m_textureManager->getViewHandle(handle);
}

ImTextureID Renderer::getEditorViewportTextureID() noexcept
{
	return (ImTextureID)(size_t)m_renderView->getResultTextureViewHandle();
}

void Renderer::setEditorMode(bool editorMode) noexcept
{
	m_editorMode = editorMode;
}

bool Renderer::isEditorMode() const noexcept
{
	return m_editorMode;
}

void Renderer::enableGridRendering(bool enable) noexcept
{
	m_renderGrid = enable;
}

bool Renderer::isGridRenderingEnabled() const noexcept
{
	return m_renderGrid;
}

void Renderer::setPickingPos(uint32_t x, uint32_t y) noexcept
{
	m_renderView->setPickingPos(x, y);
}

uint64_t Renderer::getPickedEntity() const noexcept
{
	return static_cast<uint64_t>(m_renderView->getPickedEntity());
}

void Renderer::invalidateAllReflectionProbes() noexcept
{
	m_reflectionProbeManager->invalidateAllReflectionProbes();
}

bool Renderer::startIrradianceVolumeBake() noexcept
{
	return m_irradianceVolumeManager->startBake();
}

bool Renderer::abortIrradianceVolumeBake() noexcept
{
	return m_irradianceVolumeManager->abortBake();
}

bool Renderer::isBakingIrradianceVolumes() const noexcept
{
	return m_irradianceVolumeManager->isBaking();
}

float Renderer::getIrradianceVolumeBakeProgress() const noexcept
{
	auto curVal = m_irradianceVolumeManager->getProcessedProbesCount();
	auto targetVal = m_irradianceVolumeManager->getTotalProbeCount();
	return targetVal == 0 ? 1.0f : curVal / static_cast<float>(targetVal);
}

void Renderer::clearDebugGeometry() noexcept
{
	m_renderView->clearDebugGeometry();
}

void Renderer::drawDebugLine(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec4 &color0, const glm::vec4 &color1) noexcept
{
	m_renderView->drawDebugLine(visibility, position0, position1, color0, color1);
}

void Renderer::drawDebugTriangle(DebugDrawVisibility visibility, const glm::vec3 &position0, const glm::vec3 &position1, const glm::vec3 &position2, const glm::vec4 &color0, const glm::vec4 &color1, const glm::vec4 &color2) noexcept
{
	m_renderView->drawDebugTriangle(visibility, position0, position1, position2, color0, color1, color2);
}

void Renderer::drawDebugBox(const glm::mat4 &transform, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded, bool wireframe) noexcept
{
	glm::vec3 corners[2][2][2];

	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int z = 0; z < 2; ++z)
			{
				corners[x][y][z] = transform * glm::vec4(glm::vec3(x, y, z) * 2.0f - 1.0f, 1.0f);
			}
		}
	}

	if (wireframe)
	{
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[1][0][0], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][1], corners[1][0][1], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][0][1], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][0][1], visibleColor, visibleColor);

		drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][0], corners[1][1][0], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][1], corners[1][1][1], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][1][0], corners[0][1][1], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[1][1][0], corners[1][1][1], visibleColor, visibleColor);

		drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][1][0], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[0][0][1], corners[0][1][1], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][1][0], visibleColor, visibleColor);
		drawDebugLine(DebugDrawVisibility::Visible, corners[1][0][1], corners[1][1][1], visibleColor, visibleColor);

		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[1][0][0], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][1], corners[1][0][1], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][0][1], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][0][1], occludedColor, occludedColor);

			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][0], corners[1][1][0], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][1], corners[1][1][1], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][1][0], corners[0][1][1], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[1][1][0], corners[1][1][1], occludedColor, occludedColor);

			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][1][0], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[0][0][1], corners[0][1][1], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][1][0], occludedColor, occludedColor);
			drawDebugLine(DebugDrawVisibility::Occluded, corners[1][0][1], corners[1][1][1], occludedColor, occludedColor);
		}
	}
	else
	{
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[1][0][0], corners[1][1][0], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[1][1][0], corners[0][1][0], visibleColor, visibleColor, visibleColor);

		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][1], corners[1][0][1], corners[1][1][1], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][1], corners[1][1][1], corners[0][1][1], visibleColor, visibleColor, visibleColor);

		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][1][0], corners[0][1][1], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][0][1], corners[0][1][1], visibleColor, visibleColor, visibleColor);

		drawDebugTriangle(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][1][0], corners[1][1][1], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[1][0][0], corners[1][0][1], corners[1][1][1], visibleColor, visibleColor, visibleColor);

		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][1][0], corners[0][1][1], corners[1][1][1], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][1][0], corners[1][1][0], corners[1][1][1], visibleColor, visibleColor, visibleColor);

		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[0][0][1], corners[1][0][1], visibleColor, visibleColor, visibleColor);
		drawDebugTriangle(DebugDrawVisibility::Visible, corners[0][0][0], corners[1][0][0], corners[1][0][1], visibleColor, visibleColor, visibleColor);

		if (drawOccluded)
		{
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[1][0][0], corners[1][1][0], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[1][1][0], corners[0][1][0], occludedColor, occludedColor, occludedColor);

			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][1], corners[1][0][1], corners[1][1][1], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][1], corners[1][1][1], corners[0][1][1], occludedColor, occludedColor, occludedColor);

			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][1][0], corners[0][1][1], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][0][1], corners[0][1][1], occludedColor, occludedColor, occludedColor);

			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][1][0], corners[1][1][1], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[1][0][0], corners[1][0][1], corners[1][1][1], occludedColor, occludedColor, occludedColor);

			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][1][0], corners[0][1][1], corners[1][1][1], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][1][0], corners[1][1][0], corners[1][1][1], occludedColor, occludedColor, occludedColor);

			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[0][0][1], corners[1][0][1], occludedColor, occludedColor, occludedColor);
			drawDebugTriangle(DebugDrawVisibility::Occluded, corners[0][0][0], corners[1][0][0], corners[1][0][1], occludedColor, occludedColor, occludedColor);
		}
	}
}

void Renderer::drawDebugSphere(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	const size_t segmentCount = 32;

	glm::mat4 transform = glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(scale);

	for (size_t i = 0; i < segmentCount; ++i)
	{
		float angle0 = i / (float)segmentCount * (2.0f * glm::pi<float>());
		float angle1 = (i + 1) / (float)segmentCount * (2.0f * glm::pi<float>());
		float x0 = cosf(angle0);
		float x1 = cosf(angle1);
		float y0 = sinf(angle0);
		float y1 = sinf(angle1);

		glm::vec3 p0, p1;

		// horizontal
		p0 = transform * glm::vec4(x0, 0.0f, y0, 1.0f);
		p1 = transform * glm::vec4(x1, 0.0f, y1, 1.0f);
		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}

		// vertical z
		p0 = transform * glm::vec4(0.0f, x0, y0, 1.0f);
		p1 = transform * glm::vec4(0.0f, x1, y1, 1.0f);
		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}

		// vertical x
		p0 = transform * glm::vec4(x0, y0, 0.0f, 1.0f);
		p1 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}
}

void Renderer::drawDebugCappedCone(glm::vec3 position, glm::quat rotation, float coneLength, float coneAngle, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	constexpr size_t segmentCount = 32;
	constexpr size_t capSegmentCount = 16;

	glm::mat3 rotation3x3 = glm::mat3_cast(rotation);

	// cone lines
	for (size_t i = 0; i < segmentCount; ++i)
	{
		float angle = i / (float)segmentCount * glm::two_pi<float>();
		float x = cosf(angle);
		float z = sinf(angle);

		float alpha = coneAngle * 0.5f;
		float hypotenuse = coneLength;
		float adjacent = cosf(alpha) * hypotenuse; // cosine = adjacent/ hypotenuse
		float opposite = tanf(alpha) * adjacent; // tangent = opposite / adjacent
		x *= opposite;
		z *= opposite;

		glm::vec3 p0 = position;
		glm::vec3 p1 = rotation3x3 * -glm::vec3(x, adjacent, z) + position;

		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}

	// cap
	for (size_t i = 0; i < capSegmentCount; ++i)
	{
		const float offset = 0.5f * (glm::pi<float>() - coneAngle);
		float angle0 = i / (float)capSegmentCount * coneAngle + offset;
		float angle1 = (i + 1) / (float)capSegmentCount * coneAngle + offset;
		float x0 = cosf(angle0) * coneLength;
		float x1 = cosf(angle1) * coneLength;
		float z0 = sinf(angle0) * coneLength;
		float z1 = sinf(angle1) * coneLength;

		glm::vec3 p0, p1;

		// horizontal
		p0 = rotation3x3 * -glm::vec3(x0, z0, 0.0f) + position;
		p1 = rotation3x3 * -glm::vec3(x1, z1, 0.0f) + position;
		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}

		// vertical
		p0 = rotation3x3 * -glm::vec3(0.0f, z0, x0) + position;
		p1 = rotation3x3 * -glm::vec3(0.0f, z1, x1) + position;
		drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
		if (drawOccluded)
		{
			drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
		}
	}
}

void Renderer::drawDebugArrow(glm::vec3 position, glm::quat rotation, float scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	glm::mat3 rotation3x3 = glm::mat3_cast(rotation);

	glm::vec3 arrowHead = rotation3x3 * glm::vec3(0.0f, 0.0f, 0.0f) * scale + position;
	glm::vec3 arrowTail = rotation3x3 * glm::vec3(0.0f, 1.0f, 0.0f) * scale + position;
	glm::vec3 arrowHeadX0 = rotation3x3 * glm::vec3(0.08f, 0.08f, 0.0f) * scale + position;
	glm::vec3 arrowHeadX1 = rotation3x3 * glm::vec3(-0.08f, 0.08f, 0.0f) * scale + position;
	glm::vec3 arrowHeadZ0 = rotation3x3 * glm::vec3(0.0f, 0.08f, 0.08f) * scale + position;
	glm::vec3 arrowHeadZ1 = rotation3x3 * glm::vec3(0.0f, 0.08f, -0.08f) * scale + position;

	drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowTail, visibleColor, visibleColor);
	drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadX0, visibleColor, visibleColor);
	drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadX1, visibleColor, visibleColor);
	drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadZ0, visibleColor, visibleColor);
	drawDebugLine(DebugDrawVisibility::Visible, arrowHead, arrowHeadZ1, visibleColor, visibleColor);

	if (drawOccluded)
	{
		drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowTail, occludedColor, occludedColor);
		drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadX0, occludedColor, occludedColor);
		drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadX1, occludedColor, occludedColor);
		drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadZ0, occludedColor, occludedColor);
		drawDebugLine(DebugDrawVisibility::Occluded, arrowHead, arrowHeadZ1, occludedColor, occludedColor);
	}
}

void Renderer::drawDebugCross(glm::vec3 position, float scale, const glm::vec4 &visibleColor, const glm::vec4 &occludedColor, bool drawOccluded) noexcept
{
	glm::vec3 p0;
	glm::vec3 p1;

	p0 = position + glm::vec3(scale, scale, scale);
	p1 = position + -glm::vec3(scale, scale, scale);
	drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
	if (drawOccluded)
	{
		drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
	}

	p0 = position + glm::vec3(scale, scale, -scale);
	p1 = position + -glm::vec3(scale, scale, -scale);
	drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
	if (drawOccluded)
	{
		drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
	}

	p0 = position + glm::vec3(-scale, scale, scale);
	p1 = position + -glm::vec3(-scale, scale, scale);
	drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
	if (drawOccluded)
	{
		drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
	}

	p0 = position + glm::vec3(-scale, scale, -scale);
	p1 = position + -glm::vec3(-scale, scale, -scale);
	drawDebugLine(DebugDrawVisibility::Visible, p0, p1, visibleColor, visibleColor);
	if (drawOccluded)
	{
		drawDebugLine(DebugDrawVisibility::Occluded, p0, p1, occludedColor, occludedColor);
	}
}
