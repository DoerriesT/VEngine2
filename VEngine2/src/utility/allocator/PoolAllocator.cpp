#include "PoolAllocator.h"
#include <stdlib.h>
#include <assert.h>
#include "utility/Utility.h"

static constexpr uint32_t k_invalidIndex = 0xFFFFFFFF;

static uint32_t initializeLinkedList(char *memory, size_t elementSize, size_t elementCount) noexcept
{
	assert(elementSize >= sizeof(k_invalidIndex));
	*(uint32_t *)(memory) = k_invalidIndex; // set "next" index of last item in free list to invalid index

	uint32_t prevElementIndex = 0;
	for (size_t i = 1; i < elementCount; ++i)
	{
		*(uint32_t *)(memory + i * elementSize) = prevElementIndex;
		++prevElementIndex;
	}

	return prevElementIndex;
}

FixedPoolAllocator::FixedPoolAllocator(char *memory, size_t elementSize, size_t elementCount, const char *name) noexcept
	:m_name(name),
	m_elementSize(elementSize),
	m_elementCount(elementCount),
	m_memory(memory),
	m_freeListHeadIndex(initializeLinkedList(memory, elementSize, elementCount)),
	m_ownsMemory(false)
{
}

FixedPoolAllocator::FixedPoolAllocator(size_t elementSize, size_t elementCount, const char *name) noexcept
	:m_name(name),
	m_elementSize(elementSize),
	m_elementCount(elementCount),
	m_memory((char *)malloc(elementSize * elementCount)),
	m_freeListHeadIndex(initializeLinkedList(m_memory, elementSize, elementCount)),
	m_ownsMemory(true)
{
}

FixedPoolAllocator::FixedPoolAllocator(FixedPoolAllocator &&other) noexcept
	:m_name(other.m_name),
	m_elementSize(other.m_elementSize),
	m_elementCount(other.m_elementCount),
	m_memory(other.m_memory),
	m_freeElementCount(other.m_freeElementCount),
	m_freeListHeadIndex(other.m_freeListHeadIndex),
	m_ownsMemory(other.m_ownsMemory)
{
	other.m_memory = nullptr;
	other.m_freeElementCount = 0;
	other.m_freeListHeadIndex = k_invalidIndex;
	other.m_ownsMemory = false;
}

FixedPoolAllocator::~FixedPoolAllocator()
{
	if (m_ownsMemory)
	{
		free(m_memory);
	}
}

void *FixedPoolAllocator::allocate(size_t n, int flags) noexcept
{
	if (n > m_elementSize)
	{
		return nullptr;
	}

	if (m_freeListHeadIndex != k_invalidIndex)
	{
		char *resultPtr = m_memory + m_elementSize * m_freeListHeadIndex;
		m_freeListHeadIndex = *(uint32_t *)resultPtr;
		--m_freeElementCount;
		return resultPtr;
	}

	return nullptr;
}

void *FixedPoolAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	assert((m_elementSize % alignment) == 0);
	return allocate(n, flags);
}

void FixedPoolAllocator::deallocate(void *p, size_t n) noexcept
{
	// get element index of the memory slot we are freeing
	const size_t offsetFromPoolStart = (char *)p - m_memory;
	assert((offsetFromPoolStart % m_elementSize) == 0);
	const size_t elementIndex = offsetFromPoolStart / m_elementSize;
	assert(elementIndex < m_elementCount);

	// write free list header value into our freed memory...
	uint32_t *poolFreeList = (uint32_t *)p;
	*poolFreeList = m_freeListHeadIndex;

	// ... and set our newly freed slot as head of the free list
	m_freeListHeadIndex = (uint32_t)elementIndex;
	++m_freeElementCount;
}

const char *FixedPoolAllocator::get_name() const noexcept
{
	return m_name;
}

void FixedPoolAllocator::set_name(const char *pName) noexcept
{
	m_name = pName;
}

size_t FixedPoolAllocator::getFreeElementCount() const noexcept
{
	return m_freeElementCount;
}

DynamicPoolAllocator::DynamicPoolAllocator(size_t elementSize, size_t initialElementCount, const char *name) noexcept
	:m_name(name),
	m_elementSize(elementSize),
	m_nextPoolCapacity(initialElementCount)
{
}

DynamicPoolAllocator::DynamicPoolAllocator(DynamicPoolAllocator &&other) noexcept
	:m_name(other.m_name),
	m_elementSize(other.m_elementSize),
	m_freeElementCount(other.m_freeElementCount),
	m_nextPoolCapacity(other.m_nextPoolCapacity),
	m_pools(other.m_pools)
{
	other.m_freeElementCount = 0;
	other.m_pools = nullptr;
}

DynamicPoolAllocator::~DynamicPoolAllocator()
{
	Pool *pool = m_pools;
	while (pool)
	{
		Pool *next = pool->m_nextPool;
		free(pool->m_memory);
		pool = next;
	}
}

void *DynamicPoolAllocator::allocate(size_t n, int flags) noexcept
{
	if (n > m_elementSize)
	{
		return nullptr;
	}

	// find a pool with some space
	Pool *pool = m_pools;
	while (pool)
	{
		// found a pool with free elements -> allocate and return
		if (pool->m_freeListHeadIndex != k_invalidIndex)
		{
			char *resultPtr = pool->m_memory + m_elementSize * pool->m_freeListHeadIndex;
			pool->m_freeListHeadIndex = *(uint32_t *)resultPtr;
			--(pool->m_freeElementCount);
			--m_freeElementCount;
			return resultPtr;
		}

		// pool had no free elements, go to next one
		pool = pool->m_nextPool;
	}

	// we're still here, which means we ran out of pools -> create a new one
	const size_t poolCapacity = m_nextPoolCapacity;
	m_nextPoolCapacity = m_nextPoolCapacity + (m_nextPoolCapacity / 2);
	const size_t poolElementMemorySize = poolCapacity * m_elementSize;
	const size_t poolInfoOffset = util::alignUp(poolElementMemorySize, alignof(Pool)); // place book keeping data at the end of the allocation
	
	// allocate memory for both elements and pool book keeping data
	char *poolMemory = (char *)malloc(poolInfoOffset + sizeof(Pool));

	// placement new pool at correct offset
	Pool *newPool = new (poolMemory + poolInfoOffset) Pool();
	*newPool = {};
	newPool->m_nextPool = m_pools;
	newPool->m_memory = poolMemory;
	newPool->m_elementCount = poolCapacity;
	newPool->m_freeElementCount = 0;
	newPool->m_freeListHeadIndex = initializeLinkedList(newPool->m_memory, m_elementSize, newPool->m_elementCount);

	m_freeElementCount += poolCapacity;

	// set new pool as head of list
	m_pools = newPool;

	// allocate from pool
	{
		char *resultPtr = newPool->m_memory + m_elementSize * newPool->m_freeListHeadIndex;
		newPool->m_freeListHeadIndex = *(uint32_t *)resultPtr;
		--(newPool->m_freeElementCount);
		--m_freeElementCount;
		return resultPtr;
	}
}

void *DynamicPoolAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	assert((m_elementSize % alignment) == 0);
	return allocate(n, flags);
}

void DynamicPoolAllocator::deallocate(void *p, size_t n) noexcept
{
	// find correct pool
	for (Pool *pool = m_pools; pool != nullptr; pool = pool->m_nextPool)
	{
		char *lowerBound = pool->m_memory;
		char *upperBound = pool->m_memory + pool->m_elementCount * m_elementSize;
	
		// does the pointer belong to this pool?
		if (p >= lowerBound && p < upperBound)
		{
			// get element index of the memory slot we are freeing
			const size_t offsetFromPoolStart = ((char *)p) - pool->m_memory;
			assert((offsetFromPoolStart % m_elementSize) == 0);
			const size_t elementIndex = offsetFromPoolStart / m_elementSize;
			assert(elementIndex < pool->m_elementCount);

			// write free list header value into our freed memory...
			uint32_t *poolFreeList = (uint32_t *)p;
			*poolFreeList = pool->m_freeListHeadIndex;

			// ... and set our newly freed slot as head of the free list
			pool->m_freeListHeadIndex = (uint32_t)elementIndex;

			++(pool->m_freeElementCount);
			++m_freeElementCount;

			return;
		}
	}

	// couldnt find a pool that owns this address
	assert(false);
}

const char *DynamicPoolAllocator::get_name() const noexcept
{
	return m_name;
}

void DynamicPoolAllocator::set_name(const char *pName) noexcept
{
	m_name = pName;
}

size_t DynamicPoolAllocator::getFreeElementCount() const noexcept
{
	return m_freeElementCount;
}

void DynamicPoolAllocator::clearEmptyPools() noexcept
{
	Pool **prevPoolNextPtr = &m_pools;
	Pool *pool = m_pools;
	while (pool)
	{
		Pool *nextPool = pool->m_nextPool;

		// pool is empty -> free memory and remove from linked list
		if (pool->m_elementCount == pool->m_freeElementCount)
		{
			// m_memory also holds the memory for the pool book keeping data
			free(pool->m_memory);
			
			// redirect next pointer of previous pool to skip this one
			*prevPoolNextPtr = nextPool;
		}
		// pool is not empty, update next pointer of "previous" pool for the next iteration
		else
		{
			prevPoolNextPtr = &nextPool;
		}

		pool = nextPool;
	}
}
