#pragma once
#include <stdint.h>
#include <assert.h>
#include "gal/GraphicsAbstractionLayer.h"
#include "utility/DeletedCopyMove.h"

class BufferStackAllocator
{
public:
	explicit BufferStackAllocator(gal::GraphicsDevice *device, gal::Buffer *buffer) noexcept;
	DELETED_COPY_MOVE(BufferStackAllocator);
	~BufferStackAllocator() noexcept;

	/// <summary>
	/// Attempts to allocate memory from the underlying buffer.
	/// </summary>
	/// <param name="alignment">The alignment of the allocation.</param>
	/// <param name="size">(In/Out)The requested size. On success, will hold the actual allocated size.</param>
	/// <param name="offset">(Out)The offset of the allocation inside the buffer.</param>
	/// <returns>A pointer to the allocated, mapped buffer memory on success and a nullptr otherwise.</returns>
	uint8_t *allocate(uint64_t alignment, uint64_t *size, uint64_t *offset) noexcept;
	gal::Buffer *getBuffer() const noexcept;
	void reset() noexcept;
	uint64_t getCurrentOffset() const noexcept;

	template<typename T>
	uint64_t uploadStruct(gal::DescriptorType type, const T &data)
	{
		uint64_t allocSize = sizeof(T);
		uint64_t allocOffset = 0;
		auto *mappedPtr = allocate(m_device->getBufferAlignment(type, allocSize), &allocSize, &allocOffset);
		if (mappedPtr)
		{
			memcpy(mappedPtr, &data, sizeof(data));
			return allocOffset;
		}
		
		assert(false);
		return -1;
	}

private:
	gal::GraphicsDevice *m_device = nullptr;
	uint64_t m_currentOffset = 0;
	const uint64_t m_bufferSize = 0;
	uint8_t *m_mappedPtr = nullptr;
	gal::Buffer *m_buffer = nullptr;
};