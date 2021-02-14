#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace gal
{
	class SemaphoreVk : public Semaphore
	{
	public:
		explicit SemaphoreVk(VkDevice device, uint64_t initialValue);
		SemaphoreVk(SemaphoreVk &) = delete;
		SemaphoreVk(SemaphoreVk &&) = delete;
		SemaphoreVk &operator= (const SemaphoreVk &) = delete;
		SemaphoreVk &operator= (const SemaphoreVk &&) = delete;
		~SemaphoreVk();
		void *getNativeHandle() const override;
		uint64_t getCompletedValue() const override;
		void wait(uint64_t waitValue) const override;
		void signal(uint64_t signalValue) const override;
	private:
		VkDevice m_device;
		VkSemaphore m_semaphore;
	};
}