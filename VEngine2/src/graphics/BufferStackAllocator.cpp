#include "BufferStackAllocator.h"
#include "gal/GraphicsAbstractionLayer.h"
#include <assert.h>
#include "utility/Utility.h"

BufferStackAllocator::BufferStackAllocator(gal::Buffer *buffer)
	:m_bufferSize(buffer->getDescription().m_size),
	m_buffer(buffer)
{
	m_buffer->map((void **)&m_mappedPtr);
}

BufferStackAllocator::~BufferStackAllocator()
{
	m_buffer->unmap();
}

uint8_t *BufferStackAllocator::allocate(uint64_t alignment, uint64_t *size, uint64_t *offset)
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

gal::Buffer *BufferStackAllocator::getBuffer() const
{
	return m_buffer;
}

void BufferStackAllocator::reset()
{
	m_currentOffset = 0;
}