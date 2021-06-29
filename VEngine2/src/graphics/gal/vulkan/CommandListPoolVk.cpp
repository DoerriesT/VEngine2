#include "CommandListPoolVk.h"
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include <algorithm>

gal::CommandListPoolVk::CommandListPoolVk(GraphicsDeviceVk &device, const QueueVk &queue)
	:m_device(&device),
	m_commandPool(VK_NULL_HANDLE),
	m_commandListMemoryPool(32)
{
	VkCommandPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queue.m_queueFamily };
	UtilityVk::checkResult(vkCreateCommandPool(device.getDevice(), &poolCreateInfo, nullptr, &m_commandPool), "Failed to create command pool!");
}

gal::CommandListPoolVk::~CommandListPoolVk()
{
	vkDestroyCommandPool(m_device->getDevice(), m_commandPool, nullptr);

	// normally we would have to manually call the destructor on all currently allocated command lists,
	// which would require keeping a list of all lists. since CommandListVk is a POD, we can
	// simply let the DynamicObjectPool destructor scrap the backing memory without first doing this.
}

void *gal::CommandListPoolVk::getNativeHandle() const
{
	return m_commandPool;
}

void gal::CommandListPoolVk::allocate(uint32_t count, CommandList **commandLists)
{
	constexpr uint32_t batchSize = 32;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;

	VkDevice deviceVk = m_device->getDevice();
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkCommandBuffer commandBuffers[batchSize];
		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, countVk };
		vkAllocateCommandBuffers(deviceVk, &allocInfo, commandBuffers);

		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *memory = m_commandListMemoryPool.alloc();
			assert(memory);
			commandLists[j + i * batchSize] = new(memory) CommandListVk(commandBuffers[j], m_device);
		}
	}
}

void gal::CommandListPoolVk::free(uint32_t count, CommandList **commandLists)
{
	constexpr uint32_t batchSize = 32;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;

	VkDevice deviceVk = m_device->getDevice();
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkCommandBuffer commandBuffers[batchSize];
		
		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *commandListVk = dynamic_cast<CommandListVk *>(commandLists[j + i * batchSize]);
			assert(commandListVk);
			commandBuffers[j] = (VkCommandBuffer)commandListVk->getNativeHandle();

			// call destructor and free backing memory
			commandListVk->~CommandListVk();
			m_commandListMemoryPool.free(reinterpret_cast<RawView<CommandListVk> *>(commandListVk));
		}

		vkFreeCommandBuffers(deviceVk, m_commandPool, countVk, commandBuffers);
	}
}

void gal::CommandListPoolVk::reset()
{
	vkResetCommandPool(m_device->getDevice(), m_commandPool, 0);
}
