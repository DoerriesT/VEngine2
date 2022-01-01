#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "BufferStackAllocator.h"
#include "ResourceViewRegistry.h"
#include "RendererResources.h"
#include "RenderView.h"
#include "pass/ImGuiPass.h"
#include <glm/gtc/type_ptr.hpp>
#include "imgui/imgui.h"
#include "MeshManager.h"
#include "TextureLoader.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "component/TransformComponent.h"
#include "component/CameraComponent.h"
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"
#include "RenderGraph.h"
#include "ecs/ECS.h"
#include "component/TransformComponent.h"
#include "component/SkinnedMeshComponent.h"

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

	m_device = gal::GraphicsDevice::create(windowHandle, true, gal::GraphicsBackendType::D3D12);
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

	m_renderView = new RenderView(m_device, m_viewRegistry, m_meshManager, m_materialManager, m_rendererResources, m_rendererResources->m_offsetBufferDescriptorSetLayout, width, height);

	m_imguiPass = new ImGuiPass(m_device, m_viewRegistry->getDescriptorSetLayout());
}

Renderer::~Renderer() noexcept
{
	m_device->waitIdle();
	delete m_renderGraph;
	m_renderGraph = nullptr;

	delete m_imguiPass;
	delete m_renderView;
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

	m_rendererResources->m_constantBufferStackAllocators[m_frame & 1]->reset();
	m_rendererResources->m_shaderResourceBufferStackAllocators[m_frame & 1]->reset();
	m_rendererResources->m_indexBufferStackAllocators[m_frame & 1]->reset();
	m_rendererResources->m_vertexBufferStackAllocators[m_frame & 1]->reset();

	m_viewRegistry->flushChanges();
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


		// render views
		if (cameraEntity != k_nullEntity)
		{
			m_renderView->render(deltaTime, m_time, ecs, cameraEntity, m_renderGraph);
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
					imguiPassData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferStackAllocators[m_frame & 1];
					imguiPassData.m_indexBufferAllocator = m_rendererResources->m_indexBufferStackAllocators[m_frame & 1];
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

void Renderer::setPickingPos(uint32_t x, uint32_t y) noexcept
{
	m_renderView->setPickingPos(x, y);
}

uint64_t Renderer::getPickedEntity() const noexcept
{
	return static_cast<uint64_t>(m_renderView->getPickedEntity());
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
