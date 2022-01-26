#include "LinearGPUBufferAllocator.h"
#include "gal/GraphicsAbstractionLayer.h"
#include <assert.h>
#include "utility/Utility.h"

LinearGPUBufferAllocator::LinearGPUBufferAllocator(gal::GraphicsDevice *device, gal::Buffer *buffer) noexcept
	:m_device(device),
	m_bufferSize(buffer->getDescription().m_size),
	m_buffer(buffer)
{
	m_buffer->map((void **)&m_mappedPtr);
}

LinearGPUBufferAllocator::~LinearGPUBufferAllocator() noexcept
{
	m_buffer->unmap();
}

uint8_t *LinearGPUBufferAllocator::allocate(uint64_t alignment, uint64_t *size, uint64_t *offset) noexcept
{
	assert(size && offset);
	assert(*size > 0);

	uint64_t alignedOffset = util::alignUp(m_currentOffset, alignment);

	// offset after allocation
	uint64_t newOffset = util::alignUp(alignedOffset + *size, alignment);

	if (newOffset > m_bufferSize)
	{
		return nullptr;
	}
	else
	{
		m_currentOffset = newOffset;
		*size = newOffset - alignedOffset;
		*offset = alignedOffset;
		return m_mappedPtr + alignedOffset;
	}
}

gal::Buffer *LinearGPUBufferAllocator::getBuffer() const noexcept
{
	return m_buffer;
}

void LinearGPUBufferAllocator::reset() noexcept
{
	m_currentOffset = 0;
}

uint64_t LinearGPUBufferAllocator::getCurrentOffset() const noexcept
{
	return m_currentOffset;
}
