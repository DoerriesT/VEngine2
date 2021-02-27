#pragma once
#include <stdint.h>
#include "gal/FwdDecl.h"

class BufferStackAllocator
{
public:
	explicit BufferStackAllocator(gal::Buffer *buffer);
	BufferStackAllocator(BufferStackAllocator &) = delete;
	BufferStackAllocator(BufferStackAllocator &&) = delete;
	BufferStackAllocator &operator=(const BufferStackAllocator &) = delete;
	BufferStackAllocator &operator=(const BufferStackAllocator &&) = delete;
	~BufferStackAllocator();

	/// <summary>
	/// Attempts to allocate memory from the underlying buffer.
	/// </summary>
	/// <param name="alignment">The alignment of the allocation.</param>
	/// <param name="size">(In/Out)The requested size. On success, will hold the actual allocated size.</param>
	/// <param name="offset">(Out)The offset of the allocation inside the buffer.</param>
	/// <returns>A pointer to the allocated, mapped buffer memory on success and a nullptr otherwise.</returns>
	uint8_t *allocate(uint64_t alignment, uint64_t *size, uint64_t *offset);
	gal::Buffer *getBuffer() const;
	void reset();

private:
	uint64_t m_currentOffset = 0;
	const uint64_t m_bufferSize = 0;
	uint8_t *m_mappedPtr = nullptr;
	gal::Buffer *m_buffer = nullptr;
};