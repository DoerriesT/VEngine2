#pragma once
#include <stdint.h>

class FixedPoolAllocator
{
public:
	explicit FixedPoolAllocator(char *memory, size_t elementSize, size_t elementCount, const char *name = nullptr) noexcept;
	explicit FixedPoolAllocator(size_t elementSize, size_t elementCount, const char *name = nullptr) noexcept;
	FixedPoolAllocator(FixedPoolAllocator &&other) noexcept;

	FixedPoolAllocator(const FixedPoolAllocator &) noexcept = delete;
	FixedPoolAllocator &operator=(const FixedPoolAllocator &) noexcept = delete;
	FixedPoolAllocator &operator=(FixedPoolAllocator &&other) noexcept = delete;

	~FixedPoolAllocator();

	// EASTL allocator interface:

	void *allocate(size_t n, int flags = 0) noexcept;
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept;
	void  deallocate(void *p, size_t n) noexcept;
	const char *get_name() const noexcept;
	void set_name(const char *pName) noexcept;

	size_t getFreeElementCount() const noexcept;

private:
	const char *m_name = nullptr;
	const size_t m_elementSize = 0;
	const size_t m_elementCount = 0;
	char *m_memory = nullptr;
	size_t m_freeElementCount = 0;
	uint32_t m_freeListHeadIndex = 0xFFFFFFFF;
	bool m_ownsMemory = false;
};

class DynamicPoolAllocator
{
public:
	explicit DynamicPoolAllocator(size_t elementSize, size_t initialElementCount, const char *name = nullptr) noexcept;
	DynamicPoolAllocator(DynamicPoolAllocator &&other) noexcept;

	DynamicPoolAllocator(const DynamicPoolAllocator &) noexcept = delete;
	DynamicPoolAllocator &operator=(const DynamicPoolAllocator &) noexcept = delete;
	DynamicPoolAllocator &operator=(DynamicPoolAllocator &&other) noexcept = delete;

	~DynamicPoolAllocator();

	// EASTL allocator interface:

	void *allocate(size_t n, int flags = 0) noexcept;
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept;
	void  deallocate(void *p, size_t n) noexcept;
	const char *get_name() const noexcept;
	void set_name(const char *pName) noexcept;

	size_t getFreeElementCount() const noexcept;
	void clearEmptyPools() noexcept;

private:
	struct Pool
	{
		Pool *m_nextPool = nullptr;
		char *m_memory = nullptr;
		size_t m_elementCount = 0;
		size_t m_freeElementCount = 0;
		uint32_t m_freeListHeadIndex = 0xFFFFFFFF;
	};

	const char *m_name;
	size_t m_elementSize;
	size_t m_freeElementCount = 0;
	size_t m_nextPoolCapacity = 0;
	Pool *m_pools = nullptr;
};