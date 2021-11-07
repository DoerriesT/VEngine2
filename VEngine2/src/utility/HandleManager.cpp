#include "HandleManager.h"
#include <assert.h>
#include <EASTL/algorithm.h>

HandleManager::HandleManager(uint32_t maxHandle) noexcept
	:m_maxHandle(maxHandle)
{
}

uint32_t HandleManager::allocate(bool transient) noexcept
{
	uint32_t resultHandle = 0;
	if (!m_freeHandles.empty())
	{
		resultHandle = m_freeHandles.back();
		m_freeHandles.pop_back();
	}
	else if (m_nextFreeHandle <= m_maxHandle)
	{
		resultHandle = m_nextFreeHandle++;
	}

	if (transient && resultHandle != 0)
	{
		m_transientHandles.push_back(resultHandle);
	}

	return resultHandle;
}

void HandleManager::free(uint32_t handle) noexcept
{
	if (handle != 0)
	{
		assert(isValidHandle(handle));
		m_freeHandles.push_back(handle);
	}
}

void HandleManager::freeTransientHandles() noexcept
{
	if (!m_transientHandles.empty())
	{
		m_freeHandles.insert(m_freeHandles.end(), m_transientHandles.begin(), m_transientHandles.end());
		m_transientHandles.clear();
	}
}

bool HandleManager::isValidHandle(uint32_t handle) const noexcept
{
	// null handle
	if (!handle)
	{
		return false;
	}

	// handle hasnt been allocated yet
	if (handle >= m_nextFreeHandle)
	{
		return false;
	}

	// is freed handle
	if (eastl::find(m_freeHandles.begin(), m_freeHandles.end(), handle) != m_freeHandles.end())
	{
		return false;
	}

	// is transient handle
	if (eastl::find(m_transientHandles.begin(), m_transientHandles.end(), handle) != m_transientHandles.end())
	{
		return true;
	}

	return true;
}
