#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "BufferStackAllocator.h"
#include "ResourceViewRegistry.h"
#include "RendererResources.h"
#include "RenderView.h"
#include "pass/ImGuiPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <optick.h>
#include "imgui/imgui.h"
#include "TextureLoader.h"
#include "TextureManager.h"

Renderer::Renderer(void *windowHandle, uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	m_device = gal::GraphicsDevice::create(windowHandle, true, gal::GraphicsBackendType::VULKAN);
	m_graphicsQueue = m_device->getGraphicsQueue();
	m_device->createSwapChain(m_graphicsQueue, width, height, false, gal::PresentMode::V_SYNC, &m_swapChain);
	m_device->createSemaphore(0, &m_semaphore);
	m_device->createCommandListPool(m_graphicsQueue, &m_cmdListPools[0]);
	m_device->createCommandListPool(m_graphicsQueue, &m_cmdListPools[1]);
	m_cmdListPools[0]->allocate(1, &m_cmdLists[0]);
	m_cmdListPools[1]->allocate(1, &m_cmdLists[1]);

	m_viewRegistry = new ResourceViewRegistry(m_device);
	m_rendererResources = new RendererResources(m_device, m_viewRegistry);

	m_textureLoader = new TextureLoader(m_device);
	m_textureManager = new TextureManager(m_device, m_viewRegistry);

	m_renderView = new RenderView(m_device, m_viewRegistry, m_rendererResources->m_offsetBufferDescriptorSetLayout, width, height);

	m_imguiPass = new ImGuiPass(m_device, m_viewRegistry->getDescriptorSetLayout());
}

Renderer::~Renderer()
{
	m_device->waitIdle();
	m_cmdListPools[0]->free(1, &m_cmdLists[0]);
	m_cmdListPools[1]->free(1, &m_cmdLists[1]);
	m_device->destroyCommandListPool(m_cmdListPools[0]);
	m_device->destroyCommandListPool(m_cmdListPools[1]);
	m_device->destroySemaphore(m_semaphore);
	m_device->destroySwapChain();

	delete m_imguiPass;
	delete m_renderView;
	delete m_textureLoader;
	delete m_textureManager;
	delete m_rendererResources;
	delete m_viewRegistry;

	for (size_t i = 0; i < 3; ++i)
	{
		m_device->destroyImageView(m_imageViews[i]);
		m_imageViews[i] = nullptr;
	}

	gal::GraphicsDevice::destroy(m_device);
}

void Renderer::render(const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition)
{
	m_semaphore->wait(m_waitValues[m_frame & 1]);

	m_cmdListPools[m_frame & 1]->reset();
	m_rendererResources->m_constantBufferStackAllocators[m_frame & 1]->reset();
	m_rendererResources->m_indexBufferStackAllocators[m_frame & 1]->reset();
	m_rendererResources->m_vertexBufferStackAllocators[m_frame & 1]->reset();

	m_viewRegistry->flushChanges();
	m_textureManager->flushDeletionQueue(m_frame);

	gal::CommandList *cmdList = m_cmdLists[m_frame & 1];

	cmdList->begin();
	{
		OPTICK_GPU_CONTEXT((VkCommandBuffer)cmdList->getNativeHandle());

		// upload textures
		m_textureLoader->flushUploadCopies(cmdList, m_frame);

		// render views

		m_renderView->render(cmdList, m_rendererResources->m_constantBufferStackAllocators[m_frame & 1], m_rendererResources->m_offsetBufferDescriptorSets[m_frame & 1], viewMatrix, projectionMatrix, cameraPosition, false);


		if (m_width != 0 && m_height != 0)
		{
			auto swapchainIndex = m_swapChain->getCurrentImageIndex();
			gal::Image *image = m_swapChain->getImage(swapchainIndex);
			if (!m_imageViews[swapchainIndex])
			{
				m_device->createImageView(image, &m_imageViews[swapchainIndex]);
			}

			gal::Barrier b0 = gal::Initializers::imageBarrier(
				image,
				gal::PipelineStageFlags::TOP_OF_PIPE_BIT,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::ResourceState::UNDEFINED,
				gal::ResourceState::WRITE_TRANSFER);

			cmdList->barrier(1, &b0);

			// copy view to swap chain
			{
				gal::ImageCopy imageCopy{};
				imageCopy.m_srcLayerCount = 1;
				imageCopy.m_dstLayerCount = 1;
				imageCopy.m_extent = { m_width, m_height, 1 };
				cmdList->copyImage(m_renderView->getResultImage(), image, 1, &imageCopy);
			}
			

			gal::Barrier b1 = gal::Initializers::imageBarrier(
				image,
				gal::PipelineStageFlags::TRANSFER_BIT,
				gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
				gal::ResourceState::WRITE_TRANSFER,
				gal::ResourceState::WRITE_COLOR_ATTACHMENT);

			cmdList->barrier(1, &b1);

			// imgui
			{
				ImGuiPass::Data imguiPassData{};
				imguiPassData.m_vertexBufferAllocator = m_rendererResources->m_vertexBufferStackAllocators[m_frame & 1];
				imguiPassData.m_indexBufferAllocator = m_rendererResources->m_indexBufferStackAllocators[m_frame & 1];
				imguiPassData.m_bindlessSet = m_viewRegistry->getCurrentFrameDescriptorSet();
				imguiPassData.m_width = m_width;
				imguiPassData.m_height = m_height;
				imguiPassData.m_colorAttachment = m_imageViews[swapchainIndex];
				imguiPassData.m_clear = false;
				imguiPassData.m_imGuiDrawData = ImGui::GetDrawData();

				m_imguiPass->record(cmdList, imguiPassData);
			}

			gal::Barrier b2 = gal::Initializers::imageBarrier(
				image,
				gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
				gal::PipelineStageFlags::BOTTOM_OF_PIPE_BIT,
				gal::ResourceState::WRITE_COLOR_ATTACHMENT,
				gal::ResourceState::PRESENT);

			cmdList->barrier(1, &b2);
		}

		
	}
	cmdList->end();

	++m_semaphoreValue;

	gal::SubmitInfo submitInfo{};
	submitInfo.m_commandListCount = 1;
	submitInfo.m_commandLists = &cmdList;
	submitInfo.m_signalSemaphoreCount = 1;
	submitInfo.m_signalSemaphores = &m_semaphore;
	submitInfo.m_signalValues = &m_semaphoreValue;

	m_graphicsQueue->submit(1, &submitInfo);

	OPTICK_GPU_FLIP(m_swapChain->getNativeHandle());
	if (m_width != 0 && m_height != 0)
	{
		m_swapChain->present(m_semaphore, m_semaphoreValue, m_semaphore, m_semaphoreValue + 1);
		++m_semaphoreValue;
	}
	
	m_waitValues[m_frame & 1] = m_semaphoreValue;

	m_viewRegistry->swapSets();

	++m_frame;
}

void Renderer::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	if (width != 0 && height != 0)
	{
		m_device->waitIdle();
		m_swapChain->resize(width, height, false, gal::PresentMode::IMMEDIATE);
		m_renderView->resize(width, height);
		for (size_t i = 0; i < 3; ++i)
		{
			m_device->destroyImageView(m_imageViews[i]);
			m_imageViews[i] = nullptr;
		}
	}
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

void Renderer::destroyTexture(TextureHandle handle) noexcept
{
	m_textureManager->free(handle, m_frame);
}

ImTextureID Renderer::getImGuiTextureID(TextureHandle handle) noexcept
{
	return (ImTextureID)(size_t)m_textureManager->getViewHandle(handle);
}
