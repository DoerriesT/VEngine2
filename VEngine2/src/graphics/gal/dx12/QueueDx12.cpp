#include "QueueDx12.h"
#include <vector>

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
	std::vector<ID3D12CommandList *> commandLists;

	for (size_t i = 0; i < count; ++i)
	{
		const auto &submit = submitInfo[i];

		// flush pending command lists if this submission needs to wait
		if (submit.m_waitSemaphoreCount && !commandLists.empty())
		{
			m_queue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
			commandLists.clear();
		}

		// wait on fences
		for (size_t j = 0; j < submit.m_waitSemaphoreCount; ++j)
		{
			m_queue->Wait((ID3D12Fence *)submit.m_waitSemaphores[j]->getNativeHandle(), submit.m_waitValues[j]);
		}

		// add command lists of this submission
		if (submit.m_commandListCount > 0)
		{
			commandLists.reserve(commandLists.size() + submit.m_commandListCount);

			for (size_t j = 0; j < submit.m_commandListCount; ++j)
			{
				commandLists.push_back((ID3D12CommandList *)submit.m_commandLists[j]->getNativeHandle());
			}
		}

		// flush pending command lists if this submission needs to signal
		if (submit.m_signalSemaphoreCount && !commandLists.empty())
		{
			m_queue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
			commandLists.clear();
		}
		
		// signal fences
		for (size_t j = 0; j < submit.m_signalSemaphoreCount; ++j)
		{
			m_queue->Signal((ID3D12Fence *)submit.m_signalSemaphores[j]->getNativeHandle(), submit.m_signalValues[j]);
		}
	}

	// flush remaining command lists
	if (!commandLists.empty())
	{
		m_queue->ExecuteCommandLists((UINT)commandLists.size(), commandLists.data());
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
