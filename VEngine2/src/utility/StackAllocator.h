#pragma once
#include <stdint.h>

class StackAllocator
{
public:
	typedef size_t Marker;

	explicit StackAllocator(char *memory, size_t stackSizeBytes, const char *name = nullptr) noexcept;
	explicit StackAllocator(size_t stackSizeBytes, const char *name = nullptr) noexcept;
	StackAllocator(StackAllocator &&other) noexcept;

	StackAllocator(const StackAllocator &) noexcept = delete;
	StackAllocator &operator=(const StackAllocator &) noexcept = delete;
	StackAllocator &operator=(StackAllocator &&other) noexcept = delete;

	~StackAllocator();
	
	// EASTL allocator interface:
	
	void *allocate(size_t n, int flags = 0) noexcept;
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept;
	void  deallocate(void *p, size_t n) noexcept;
	const char *get_name() const noexcept;
	void set_name(const char *pName) noexcept;


	Marker getMarker() noexcept;
	void freeToMarker(Marker marker) noexcept;
	void reset() noexcept;

private:
	const char *m_name = nullptr;
	const size_t m_stackSizeBytes = 0;
	char *m_memory = nullptr;
	size_t m_currentOffset = 0;
	bool m_ownsMemory = false;
};