#include "QueueDx12.h"
#include "utility/Memory.h"

void gal::QueueDx12::init(ID3D12CommandQueue *queue, QueueType queueType, uint32_t timestampBits, bool presentable, Semaphore *waitIdleSemaphore)
{
	m_queue = queue;
	m_queueType = queueType;
	m_timestampValidBits = timestampBits;

	uint64_t timestampFrequency;
	queue->GetTimestampFrequency(&timestampFrequency);
	m_timestampPeriod = (float)((1.0 / double(timestampFrequency)) * 1e9);
	m_presentable = presentable;
	m_waitIdleSemaphore = waitIdleSemaphore;
	m_semaphoreValue = 0;
}

void *gal::QueueDx12::getNativeHandle() const
{
	return m_queue;
}

gal::QueueType gal::QueueDx12::getQueueType() const
{
	return m_queueType;
}

uint32_t gal::QueueDx12::getTimestampValidBits() const
{
	return m_timestampValidBits;
}

float gal::QueueDx12::getTimestampPeriod() const
{
	return m_timestampPeriod;
}

bool gal::QueueDx12::canPresent() const
{
	return m_presentable;
}

void gal::QueueDx12::submit(uint32_t count, const SubmitInfo *submitInfo)
{
	// find worst case command list count
	uint32_t worstCaseCommandListCount = 0;
	for (size_t i = 0; i < count; ++i)
	{
		worstCaseCommandListCount += submitInfo[i].m_commandListCount;
	}

	ID3D12CommandList **commandLists = ALLOC_A_T(ID3D12CommandList *, worstCaseCommandListCount);
	size_t commandListCount = 0;

	for (size_t i = 0; i < count; ++i)
	{
		const auto &submit = submitInfo[i];

		// flush pending command lists if this submission needs to wait
		if (submit.m_waitSemaphoreCount && (commandListCount > 0))
		{
			m_queue->ExecuteCommandLists((UINT)commandListCount, commandLists);
			commandListCount = 0;
		}

		// wait on fences
		for (size_t j = 0; j < submit.m_waitSemaphoreCount; ++j)
		{
			m_queue->Wait((ID3D12Fence *)submit.m_waitSemaphores[j]->getNativeHandle(), submit.m_waitValues[j]);
		}

		// add command lists of this submission
		if (submit.m_commandListCount > 0)
		{
			for (size_t j = 0; j < submit.m_commandListCount; ++j)
			{
				commandLists[commandListCount++] = (ID3D12CommandList *)submit.m_commandLists[j]->getNativeHandle();
			}
		}

		// flush pending command lists if this submission needs to signal
		if (submit.m_signalSemaphoreCount && (commandListCount > 0))
		{
			m_queue->ExecuteCommandLists((UINT)commandListCount, commandLists);
			commandListCount = 0;
		}
		
		// signal fences
		for (size_t j = 0; j < submit.m_signalSemaphoreCount; ++j)
		{
			m_queue->Signal((ID3D12Fence *)submit.m_signalSemaphores[j]->getNativeHandle(), submit.m_signalValues[j]);
		}
	}

	// flush remaining command lists
	if (commandListCount > 0)
	{
		m_queue->ExecuteCommandLists((UINT)commandListCount, commandLists);
	}
}

void gal::QueueDx12::waitIdle() const
{
	++m_semaphoreValue;
	m_queue->Signal((ID3D12Fence *)m_waitIdleSemaphore->getNativeHandle(), m_semaphoreValue);
	m_waitIdleSemaphore->wait(m_semaphoreValue);
}

gal::Semaphore *gal::QueueDx12::getWaitIdleSemaphore()
{
	return m_waitIdleSemaphore;
}
