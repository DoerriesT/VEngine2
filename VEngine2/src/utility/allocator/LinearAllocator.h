#pragma once
#include <stdint.h>
#include "IAllocator.h"

class LinearAllocator : public IAllocator
{
public:
	typedef size_t Marker;

	explicit LinearAllocator(char *memory, size_t stackSizeBytes, const char *name = nullptr) noexcept;
	explicit LinearAllocator(size_t stackSizeBytes, const char *name = nullptr) noexcept;
	LinearAllocator(LinearAllocator &&other) noexcept;

	LinearAllocator(const LinearAllocator &) noexcept = delete;
	LinearAllocator &operator=(const LinearAllocator &) noexcept = delete;
	LinearAllocator &operator=(LinearAllocator &&other) noexcept = delete;

	~LinearAllocator();
	
	// EASTL allocator interface:
	
	void *allocate(size_t n, int flags = 0) noexcept override;
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept override;
	void  deallocate(void *p, size_t n) noexcept override;
	const char *get_name() const noexcept override;
	void set_name(const char *pName) noexcept override;


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