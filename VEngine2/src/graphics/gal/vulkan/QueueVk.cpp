#include "QueueVk.h"
#include "UtilityVk.h"
#include "utility/Memory.h"

void *gal::QueueVk::getNativeHandle() const
{
	return m_queue;
}

gal::QueueType gal::QueueVk::getQueueType() const
{
	return m_queueType;
}

uint32_t gal::QueueVk::getTimestampValidBits() const
{
	return m_timestampValidBits;
}

float gal::QueueVk::getTimestampPeriod() const
{
	return m_timestampPeriod;
}

bool gal::QueueVk::canPresent() const
{
	return m_presentable;
}

void gal::QueueVk::submit(uint32_t count, const SubmitInfo *submitInfo)
{
	// compute number of involved semaphores (both wait and signal), number of command buffers and number of wait dst stage masks
	size_t semaphoreCount = 0;
	size_t commandBufferCount = 0;
	size_t waitMaskCount = 0;
	for (uint32_t i = 0; i < count; ++i)
	{
		semaphoreCount += submitInfo[i].m_waitSemaphoreCount + submitInfo[i].m_signalSemaphoreCount;
		waitMaskCount += submitInfo[i].m_waitSemaphoreCount;
		commandBufferCount += submitInfo[i].m_commandListCount;
	}

	// allocate arrays
	VkSubmitInfo *submitInfoVk = ALLOC_A_T(VkSubmitInfo, count);
	VkTimelineSemaphoreSubmitInfo *timelineSemaphoreInfoVk = ALLOC_A_T(VkTimelineSemaphoreSubmitInfo, count);
	VkSemaphore *semaphoresVk = ALLOC_A_T(VkSemaphore, semaphoreCount);
	VkPipelineStageFlags *waitDstStageMasksVk = ALLOC_A_T(VkPipelineStageFlags, waitMaskCount);
	VkCommandBuffer *commandBuffersVk = ALLOC_A_T(VkCommandBuffer, commandBufferCount);

	// keep track of the current offset into the secondary arrays. submitInfoVk and timelineSemaphoreInfoVk can be indexed with i
	size_t semaphoreCurOffset = 0;
	size_t commandBuffersCurOffset = 0;
	size_t waitMaskCurOffset = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		// store the base offset into the secondary arrays for this submit info
		const size_t waitSemaphoreSubmitInfoOffset = semaphoreCurOffset;
		const size_t commandBuffersSubmitInfoOffset = commandBuffersCurOffset;
		const size_t waitMaskSubmitInfoOffset = waitMaskCurOffset;

		// copy wait semaphores and wait stage masks
		for (uint32_t j = 0; j < submitInfo[i].m_waitSemaphoreCount; ++j)
		{
			semaphoresVk[semaphoreCurOffset++] = (VkSemaphore)submitInfo[i].m_waitSemaphores[j]->getNativeHandle();
			waitDstStageMasksVk[waitMaskCurOffset++] = UtilityVk::translatePipelineStageFlags(submitInfo[i].m_waitDstStageMask[j]);
		}

		// copy command buffers
		for (uint32_t j = 0; j < submitInfo[i].m_commandListCount; ++j)
		{
			commandBuffersVk[commandBuffersCurOffset++] = (VkCommandBuffer)submitInfo[i].m_commandLists[j]->getNativeHandle();
		}

		// store the base offset into the semaphore array for signal semaphores (both wait and signal semaphores share the same array)
		const size_t signalSemaphoreSubmitInfoOffset = semaphoreCurOffset;

		// copy signal semaphores
		for (uint32_t j = 0; j < submitInfo[i].m_signalSemaphoreCount; ++j)
		{
			semaphoresVk[semaphoreCurOffset++] = (VkSemaphore)submitInfo[i].m_signalSemaphores[j]->getNativeHandle();
		}

		auto &timelineSubInfo = timelineSemaphoreInfoVk[i];
		timelineSubInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubInfo.waitSemaphoreValueCount = submitInfo[i].m_waitSemaphoreCount;
		timelineSubInfo.pWaitSemaphoreValues = submitInfo[i].m_waitValues;
		timelineSubInfo.signalSemaphoreValueCount = submitInfo[i].m_signalSemaphoreCount;
		timelineSubInfo.pSignalSemaphoreValues = submitInfo[i].m_signalValues;

		auto &subInfo = submitInfoVk[i];
		subInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		subInfo.pNext = &timelineSubInfo;
		subInfo.waitSemaphoreCount = submitInfo[i].m_waitSemaphoreCount;
		subInfo.pWaitSemaphores = semaphoresVk + waitSemaphoreSubmitInfoOffset;
		subInfo.pWaitDstStageMask = waitDstStageMasksVk + waitMaskSubmitInfoOffset;
		subInfo.commandBufferCount = submitInfo[i].m_commandListCount;
		subInfo.pCommandBuffers = commandBuffersVk + commandBuffersSubmitInfoOffset;
		subInfo.signalSemaphoreCount = submitInfo[i].m_signalSemaphoreCount;
		subInfo.pSignalSemaphores = semaphoresVk + signalSemaphoreSubmitInfoOffset;
	}

	UtilityVk::checkResult(vkQueueSubmit(m_queue, count, submitInfoVk, VK_NULL_HANDLE), "Failed to submit to Queue!");
}

void gal::QueueVk::waitIdle() const
{
	vkQueueWaitIdle(m_queue);
}
