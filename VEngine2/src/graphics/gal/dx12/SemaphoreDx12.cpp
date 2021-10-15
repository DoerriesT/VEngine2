#include "SemaphoreDx12.h"
#include "UtilityDx12.h"

gal::SemaphoreDx12::SemaphoreDx12(ID3D12Device *device, uint64_t initialValue)
	:m_fence()
{
	UtilityDx12::checkResult(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void **)&m_fence), "Failed to create semaphore!");

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		UtilityDx12::checkResult(HRESULT_FROM_WIN32(GetLastError()), "Failed to create Event for ID3D12Fence!");
	}
}

gal::SemaphoreDx12::~SemaphoreDx12()
{
	D3D12_SAFE_RELEASE(m_fence);
	CloseHandle(m_fenceEvent);
}

void *gal::SemaphoreDx12::getNativeHandle() const
{
	return m_fence;
}

uint64_t gal::SemaphoreDx12::getCompletedValue() const
{
	return m_fence->GetCompletedValue();
}

void gal::SemaphoreDx12::wait(uint64_t waitValue) const
{
	if (m_fence->GetCompletedValue() < waitValue)
	{
		m_fence->SetEventOnCompletion(waitValue, m_fenceEvent);
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}
}

void gal::SemaphoreDx12::signal(uint64_t signalValue) const
{
	m_fence->Signal(signalValue);
}
