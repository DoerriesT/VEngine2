#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include <d3d12.h>

namespace gal
{
	class SemaphoreDx12 : public Semaphore
	{
	public:
		explicit SemaphoreDx12(ID3D12Device *device, uint64_t initialValue);
		SemaphoreDx12(SemaphoreDx12 &) = delete;
		SemaphoreDx12(SemaphoreDx12 &&) = delete;
		SemaphoreDx12 &operator= (const SemaphoreDx12 &) = delete;
		SemaphoreDx12 &operator= (const SemaphoreDx12 &&) = delete;
		~SemaphoreDx12();
		void *getNativeHandle() const override;
		uint64_t getCompletedValue() const override;
		void wait(uint64_t waitValue) const override;
		void signal(uint64_t signalValue) const override;
	private:
		ID3D12Fence *m_fence;
		HANDLE m_fenceEvent;
	};
}