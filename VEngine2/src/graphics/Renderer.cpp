#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"
#include "BufferStackAllocator.h"
#include "pass/GridPass.h"
#include <glm/gtc/type_ptr.hpp>
#include <optick.h>

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

	{
		gal::BufferCreateInfo createInfo{ 1024 * 1024 * 4, {}, gal::BufferUsageFlags::CONSTANT_BUFFER_BIT };
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[0]);
		m_device->createBuffer(createInfo, gal::MemoryPropertyFlags::HOST_VISIBLE_BIT | gal::MemoryPropertyFlags::HOST_COHERENT_BIT, gal::MemoryPropertyFlags::DEVICE_LOCAL_BIT, false, &m_mappableConstantBuffers[1]);

		m_constantBufferStackAllocators[0] = new BufferStackAllocator(m_mappableConstantBuffers[0]);
		m_constantBufferStackAllocators[1] = new BufferStackAllocator(m_mappableConstantBuffers[1]);
	}
	

	gal::DescriptorSetLayoutBinding binding{ gal::DescriptorType::OFFSET_CONSTANT_BUFFER, 0, 0, 1, gal::ShaderStageFlags::ALL_STAGES };
	m_device->createDescriptorSetLayout(1, &binding, &m_offsetBufferDescriptorSetLayout);
	m_device->createDescriptorSetPool(2, m_offsetBufferDescriptorSetLayout, &m_offsetBufferDescriptorSetPool);
	m_offsetBufferDescriptorSetPool->allocateDescriptorSets(2, m_offsetBufferDescriptorSets);

	for (size_t i = 0; i < 2; ++i)
	{
		gal::DescriptorSetUpdate update{0, 0, 1, gal::DescriptorType::OFFSET_CONSTANT_BUFFER };
		update.m_bufferInfo1 = { m_mappableConstantBuffers[i], 0, 1024/*m_mappableConstantBuffers[i]->getDescription().m_size*/, 0 };
		m_offsetBufferDescriptorSets[i]->update(1, &update);
	}

	m_gridPass = new GridPass(m_device, m_offsetBufferDescriptorSetLayout);
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

	delete m_constantBufferStackAllocators[0];
	delete m_constantBufferStackAllocators[1];

	m_device->destroyBuffer(m_mappableConstantBuffers[0]);
	m_device->destroyBuffer(m_mappableConstantBuffers[1]);

	m_device->destroyDescriptorSetPool(m_offsetBufferDescriptorSetPool);
	m_device->destroyDescriptorSetLayout(m_offsetBufferDescriptorSetLayout);

	delete m_gridPass;

	gal::GraphicsDevice::destroy(m_device);
}

void Renderer::render(const float *viewMatrix, const float *projectionMatrix, const float *cameraPosition)
{
	m_semaphore->wait(m_waitValues[m_frame & 1]);

	m_cmdListPools[m_frame & 1]->reset();
	m_constantBufferStackAllocators[m_frame & 1]->reset();

	gal::CommandList *cmdList = m_cmdLists[m_frame & 1];

	cmdList->begin();
	{
		OPTICK_GPU_CONTEXT((VkCommandBuffer)cmdList->getNativeHandle());
		auto swapchainIndex = m_swapChain->getCurrentImageIndex();
		gal::Image *image = m_swapChain->getImage(swapchainIndex);
		if (!m_imageViews[swapchainIndex])
		{
			m_device->createImageView(image, &m_imageViews[swapchainIndex]);
		}

		gal::Barrier b0 = gal::Initializers::imageBarrier(
			image,
			gal::PipelineStageFlags::TOP_OF_PIPE_BIT,
			gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
			gal::ResourceState::UNDEFINED,
			gal::ResourceState::WRITE_COLOR_ATTACHMENT);

		cmdList->barrier(1, &b0);

		GridPass::Data gridPassData{};
		gridPassData.m_bufferAllocator = m_constantBufferStackAllocators[m_frame & 1];
		gridPassData.m_offsetBufferSet = m_offsetBufferDescriptorSets[m_frame & 1];
		gridPassData.m_width = m_width;
		gridPassData.m_height = m_height;
		gridPassData.m_colorAttachment = m_imageViews[m_swapChain->getCurrentImageIndex()];
		gridPassData.m_modelMatrix = glm::mat4(1.0f);
		gridPassData.m_viewProjectionMatrix = glm::make_mat4(projectionMatrix) * glm::make_mat4(viewMatrix);
		gridPassData.m_thinLineColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
		gridPassData.m_thickLineColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
		gridPassData.m_cameraPos = glm::make_vec3(cameraPosition);
		gridPassData.m_cellSize = 1.0f;
		gridPassData.m_gridSize = 1000.0f;

		m_gridPass->record(cmdList, gridPassData);

		gal::Barrier b1 = gal::Initializers::imageBarrier(
			image,
			gal::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT_BIT,
			gal::PipelineStageFlags::BOTTOM_OF_PIPE_BIT,
			gal::ResourceState::WRITE_COLOR_ATTACHMENT,
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

	OPTICK_GPU_FLIP(m_swapChain->getNativeHandle());
	m_swapChain->present(m_semaphore, m_semaphoreValue, m_semaphore, m_semaphoreValue + 1);
	++m_semaphoreValue;
	m_waitValues[m_frame & 1] = m_semaphoreValue;

	++m_frame;
}

void Renderer::resize(uint32_t width, uint32_t height)
{
	m_device->waitIdle();
	m_swapChain->resize(width, height, false, gal::PresentMode::IMMEDIATE);
	m_width = width;
	m_height = height;
	for (size_t i = 0; i < 3; ++i)
	{
		m_device->destroyImageView(m_imageViews[i]);
		m_imageViews[i] = nullptr;
	}
}
