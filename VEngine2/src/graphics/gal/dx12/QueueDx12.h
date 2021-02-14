#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace gal
{
	class QueueDx12 : public Queue
	{
	public:
		void init(ID3D12CommandQueue *queue, QueueType queueType, uint32_t timestampBits, bool presentable, Semaphore *waitIdleSemaphore);
		void *getNativeHandle() const override;
		QueueType getQueueType() const override;
		uint32_t getTimestampValidBits() const override;
		float getTimestampPeriod() const override;
		bool canPresent() const override;
		void submit(uint32_t count, const SubmitInfo *submitInfo) override;
		void waitIdle() const override;
		Semaphore *getWaitIdleSemaphore();

	private:
		ID3D12CommandQueue *m_queue;
		QueueType m_queueType;
		uint32_t m_timestampValidBits;
		float m_timestampPeriod;
		bool m_presentable;
		Semaphore *m_waitIdleSemaphore;
		mutable uint64_t m_semaphoreValue;
	};
}