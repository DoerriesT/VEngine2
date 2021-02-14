#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"

Renderer::Renderer(void *windowHandle, uint32_t width, uint32_t height)
{
	m_device = gal::GraphicsDevice::create(windowHandle, true, gal::GraphicsBackendType::VULKAN);
	m_graphicsQueue = m_device->getGraphicsQueue();
	m_device->createSwapChain(m_graphicsQueue, width, height, false, gal::PresentMode::IMMEDIATE, &m_swapChain);
	m_device->createSemaphore(0, &m_semaphore);
	m_device->createCommandListPool(m_graphicsQueue, &m_cmdListPools[0]);
	m_device->createCommandListPool(m_graphicsQueue, &m_cmdListPools[1]);
	m_cmdListPools[0]->allocate(1, &m_cmdLists[0]);
	m_cmdListPools[1]->allocate(1, &m_cmdLists[1]);
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
	gal::GraphicsDevice::destroy(m_device);
}

void Renderer::render()
{
	m_semaphore->wait(m_waitValues[m_frame & 1]);

	m_cmdListPools[m_frame & 1]->reset();

	gal::CommandList *cmdList = m_cmdLists[m_frame & 1];

	cmdList->begin();
	{
		gal::Image *image = m_swapChain->getImage(m_swapChain->getCurrentImageIndex());
		
		gal::Barrier b0 = gal::Initializers::imageBarrier(
			image, 
			gal::PipelineStageFlags::TOP_OF_PIPE_BIT, 
			gal::PipelineStageFlags::CLEAR_BIT, 
			gal::ResourceState::UNDEFINED, 
			gal::ResourceState::CLEAR_RESOURCE);

		cmdList->barrier(1, &b0);

		gal::ClearColorValue clearValue{};
		clearValue.m_float32[0] = 1.0f;
		gal::ImageSubresourceRange range{ 0, 1, 0, 1 };
		cmdList->clearColorImage(image, &clearValue, 1, &range);

		gal::Barrier b1 = gal::Initializers::imageBarrier(
			image,
			gal::PipelineStageFlags::CLEAR_BIT,
			gal::PipelineStageFlags::BOTTOM_OF_PIPE_BIT,
			gal::ResourceState::CLEAR_RESOURCE,
			gal::ResourceState::PRESENT);

		cmdList->barrier(1, &b1);
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
	

	m_swapChain->present(m_semaphore, m_semaphoreValue, m_semaphore, m_semaphoreValue + 1);
	++m_semaphoreValue;
	m_waitValues[m_frame & 1] = m_semaphoreValue;

	++m_frame;
}

void Renderer::resize(uint32_t width, uint32_t height)
{
	m_device->waitIdle();
	m_swapChain->resize(width, height, false, gal::PresentMode::IMMEDIATE);
}
