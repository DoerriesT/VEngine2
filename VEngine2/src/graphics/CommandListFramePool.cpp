#include "CommandListFramePool.h"
#include "gal/GraphicsAbstractionLayer.h"

using namespace gal;

CommandListFramePool::~CommandListFramePool() noexcept
{
	reset();
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandListPools[i])
		{
			m_device->destroyCommandListPool(m_commandListPools[i]);
		}
	}
}

void CommandListFramePool::init(gal::GraphicsDevice *device, uint32_t allocationChunkSize) noexcept
{
	m_device = device;
	m_allocationChunkSize = allocationChunkSize;
}

gal::CommandList *CommandListFramePool::acquire(Queue *queue) noexcept
{
	const uint32_t poolIndex = queue == m_device->getGraphicsQueue() ? 0 : queue == m_device->getComputeQueue() ? 1 : 2;

	if (!m_commandListPools[poolIndex])
	{
		m_device->createCommandListPool(queue, &m_commandListPools[poolIndex]);
	}

	const size_t currentPoolSize = m_commandLists[poolIndex].size();
	if (m_nextFreeCmdList[poolIndex] == currentPoolSize)
	{
		m_commandLists[poolIndex].resize(currentPoolSize + m_allocationChunkSize);

		m_commandListPools[poolIndex]->allocate(m_allocationChunkSize, m_commandLists[poolIndex].data() + currentPoolSize);
	}

	return m_commandLists[poolIndex][m_nextFreeCmdList[poolIndex]++];
}

void CommandListFramePool::reset() noexcept
{
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_commandListPools[i])
		{
			m_commandListPools[i]->reset();
		}
		m_nextFreeCmdList[i] = 0;
	}
}
