#include "LinearAllocator.h"
#include <stdlib.h>
#include "utility/Utility.h"

LinearAllocator::LinearAllocator(char *memory, size_t stackSizeBytes, const char *name) noexcept
	:m_name(name),
	m_stackSizeBytes(stackSizeBytes),
	m_memory(memory),
	m_ownsMemory(false)
{
}

LinearAllocator::LinearAllocator(size_t stackSizeBytes, const char *name) noexcept
	:m_name(name),
	m_stackSizeBytes(stackSizeBytes),
	m_memory((char *)malloc(stackSizeBytes)),
	m_ownsMemory(true)
{
}

LinearAllocator::LinearAllocator(LinearAllocator &&other) noexcept
	:m_name(other.m_name),
	m_stackSizeBytes(other.m_stackSizeBytes),
	m_memory(other.m_memory),
	m_currentOffset(other.m_currentOffset),
	m_ownsMemory(other.m_ownsMemory)
{
	other.m_memory = nullptr;
	other.m_currentOffset = 0;
	other.m_ownsMemory = false;
}

LinearAllocator::~LinearAllocator()
{
	if (m_ownsMemory)
	{
		free(m_memory);
	}
}

void *LinearAllocator::allocate(size_t n, int flags) noexcept
{
	size_t newOffset = m_currentOffset + n;
	if (newOffset <= m_stackSizeBytes)
	{
		char *resultPtr = m_memory + m_currentOffset;
		m_currentOffset = newOffset;

		return resultPtr;
	}
	return nullptr;
}

void *LinearAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	size_t curAlignedOffset = util::alignPow2Up(m_currentOffset, alignment);
	size_t newOffset = curAlignedOffset + n;

	if (newOffset <= m_stackSizeBytes)
	{
		char *resultPtr = m_memory + curAlignedOffset + offset;
		m_currentOffset = newOffset;

		return resultPtr;
	}
	return nullptr;
}

void LinearAllocator::deallocate(void *p, size_t n) noexcept
{
	// freeing individual allocations is not supported
}

const char *LinearAllocator::get_name() const noexcept
{
	return m_name;
}

void LinearAllocator::set_name(const char *pName) noexcept
{
	m_name = pName;
}

LinearAllocator::Marker LinearAllocator::getMarker() noexcept
{
	return m_currentOffset;
}

void LinearAllocator::freeToMarker(Marker marker) noexcept
{
	assert(marker <= m_currentOffset);
	m_currentOffset = marker;
}

void LinearAllocator::reset() noexcept
{
	m_currentOffset = 0;
}

LinearAllocatorFrame::LinearAllocatorFrame(LinearAllocator *allocator, const char *name) noexcept
	:m_allocator(allocator),
	m_name(name),
	m_startMarker(allocator->getMarker())
{
}

LinearAllocatorFrame::~LinearAllocatorFrame() noexcept
{
	m_allocator->freeToMarker(m_startMarker);
}

void *LinearAllocatorFrame::allocate(size_t n, int flags) noexcept
{
	return m_allocator->allocate(n, flags);
}

void *LinearAllocatorFrame::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	return m_allocator->allocate(n, alignment, offset, flags);
}

void LinearAllocatorFrame::deallocate(void *p, size_t n) noexcept
{
	m_allocator->deallocate(p, n);
}

const char *LinearAllocatorFrame::get_name() const noexcept
{
	return m_name;
}

void LinearAllocatorFrame::set_name(const char *pName) noexcept
{
	m_name = pName;
}
