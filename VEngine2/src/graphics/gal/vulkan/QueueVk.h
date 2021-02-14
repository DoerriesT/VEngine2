#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace gal
{
	class QueueVk : public Queue
	{
	public:
		void *getNativeHandle() const override;
		QueueType getQueueType() const override;
		uint32_t getTimestampValidBits() const override;
		float getTimestampPeriod() const override;
		bool canPresent() const override;
		void submit(uint32_t count, const SubmitInfo *submitInfo) override;
		void waitIdle() const override;

		VkQueue m_queue = VK_NULL_HANDLE;
		QueueType m_queueType;
		uint32_t m_timestampValidBits = 0;
		float m_timestampPeriod = 1.0f;
		bool m_presentable = false;
		uint32_t m_queueFamily = -1;
	};
}