#include "QueueVk.h"
#include <vector>
#include "UtilityVk.h"

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
	size_t semaphoreCount = 0;
	size_t commandBufferCount = 0;
	size_t waitMaskCount = 0;
	for (uint32_t i = 0; i < count; ++i)
	{
		semaphoreCount += submitInfo[i].m_waitSemaphoreCount + submitInfo[i].m_signalSemaphoreCount;
		waitMaskCount += submitInfo[i].m_waitSemaphoreCount;
		commandBufferCount += submitInfo[i].m_commandListCount;
	}

	// TODO: avoid dynamic heap allocation
	std::vector<VkSubmitInfo> submitInfoVk(count);
	std::vector<VkTimelineSemaphoreSubmitInfo> timelineSemaphoreInfoVk(count);
	std::vector<VkSemaphore> semaphores;
	semaphores.reserve(semaphoreCount);
	std::vector<VkPipelineStageFlags> waitDstStageMasks;
	waitDstStageMasks.reserve(waitMaskCount);
	std::vector<VkCommandBuffer> commandBuffers;
	commandBuffers.reserve(commandBufferCount);

	for (uint32_t i = 0; i < count; ++i)
	{
		size_t waitSemaphoreOffset = semaphores.size();
		size_t commandBuffersOffset = commandBuffers.size();
		size_t waitMaskOffset = waitDstStageMasks.size();

		for (uint32_t j = 0; j < submitInfo[i].m_waitSemaphoreCount; ++j)
		{
			semaphores.push_back((VkSemaphore)submitInfo[i].m_waitSemaphores[j]->getNativeHandle());
			waitDstStageMasks.push_back(UtilityVk::translatePipelineStageFlags(submitInfo[i].m_waitDstStageMask[j]));
		}

		for (uint32_t j = 0; j < submitInfo[i].m_commandListCount; ++j)
		{
			commandBuffers.push_back((VkCommandBuffer)submitInfo[i].m_commandLists[j]->getNativeHandle());
		}

		size_t signalSemaphoreOffset = semaphores.size();

		for (uint32_t j = 0; j < submitInfo[i].m_signalSemaphoreCount; ++j)
		{
			semaphores.push_back((VkSemaphore)submitInfo[i].m_signalSemaphores[j]->getNativeHandle());
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
		subInfo.pWaitSemaphores = semaphores.data() + waitSemaphoreOffset;
		subInfo.pWaitDstStageMask = waitDstStageMasks.data() + waitMaskOffset;
		subInfo.commandBufferCount = submitInfo[i].m_commandListCount;
		subInfo.pCommandBuffers = commandBuffers.data() + commandBuffersOffset;
		subInfo.signalSemaphoreCount = submitInfo[i].m_signalSemaphoreCount;
		subInfo.pSignalSemaphores = semaphores.data() + signalSemaphoreOffset;
	}

	UtilityVk::checkResult(vkQueueSubmit(m_queue, count, submitInfoVk.data(), VK_NULL_HANDLE), "Failed to submit to Queue!");
}

void gal::QueueVk::waitIdle() const
{
	vkQueueWaitIdle(m_queue);
}
