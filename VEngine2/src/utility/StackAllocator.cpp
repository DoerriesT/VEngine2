#include "StackAllocator.h"
#include <stdlib.h>
#include "Utility.h"

StackAllocator::StackAllocator(char *memory, size_t stackSizeBytes, const char *name) noexcept
	:m_name(name),
	m_stackSizeBytes(stackSizeBytes),
	m_memory(memory),
	m_ownsMemory(false)
{
}

StackAllocator::StackAllocator(size_t stackSizeBytes, const char *name) noexcept
	:m_name(name),
	m_stackSizeBytes(stackSizeBytes),
	m_memory((char *)malloc(stackSizeBytes)),
	m_ownsMemory(true)
{
}

StackAllocator::StackAllocator(StackAllocator &&other) noexcept
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

StackAllocator::~StackAllocator()
{
	if (m_ownsMemory)
	{
		free(m_memory);
	}
}

void *StackAllocator::allocate(size_t n, int flags) noexcept
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

void *StackAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
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

void StackAllocator::deallocate(void *p, size_t n) noexcept
{
	// freeing individual allocations is not supported
}

const char *StackAllocator::get_name() const noexcept
{
	return m_name;
}

void StackAllocator::set_name(const char *pName) noexcept
{
	m_name = pName;
}

StackAllocator::Marker StackAllocator::getMarker() noexcept
{
	return m_currentOffset;
}

void StackAllocator::freeToMarker(Marker marker) noexcept
{
	assert(marker <= m_currentOffset);
	m_currentOffset = marker;
}

void StackAllocator::reset() noexcept
{
	m_currentOffset = 0;
}
