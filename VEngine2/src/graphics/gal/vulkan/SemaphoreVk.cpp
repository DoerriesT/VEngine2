#include "SemaphoreVk.h"
#include "UtilityVk.h"
#include <limits>

gal::SemaphoreVk::SemaphoreVk(VkDevice device, uint64_t initialValue)
	:m_device(device),
	m_semaphore(VK_NULL_HANDLE)
{
	VkSemaphoreTypeCreateInfo typeCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeCreateInfo.initialValue = initialValue;

	VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeCreateInfo };

	UtilityVk::checkResult(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_semaphore), "Failed to create semaphore!");
}

gal::SemaphoreVk::~SemaphoreVk()
{
	vkDestroySemaphore(m_device, m_semaphore, nullptr);
}

void *gal::SemaphoreVk::getNativeHandle() const
{
	return m_semaphore;
}

uint64_t gal::SemaphoreVk::getCompletedValue() const
{
	uint64_t value = 0;
	UtilityVk::checkResult(vkGetSemaphoreCounterValue(m_device, m_semaphore, &value), "Failed to get semaphore counter value!");
	return value;
}

void gal::SemaphoreVk::wait(uint64_t waitValue) const
{
	VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	waitInfo.flags = VK_SEMAPHORE_WAIT_ANY_BIT;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores = &m_semaphore;
	waitInfo.pValues = &waitValue;

	UtilityVk::checkResult(vkWaitSemaphores(m_device, &waitInfo, std::numeric_limits<uint64_t>::max()), "Failed to wait on semaphore!");
}

void gal::SemaphoreVk::signal(uint64_t signalValue) const
{
	VkSemaphoreSignalInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
	signalInfo.semaphore = m_semaphore;
	signalInfo.value = signalValue;

	UtilityVk::checkResult(vkSignalSemaphore(m_device, &signalInfo), "Failed to signal semaphore!");
}
