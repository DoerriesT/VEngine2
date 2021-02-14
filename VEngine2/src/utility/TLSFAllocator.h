#pragma once
#include <cstdint>
#include "ObjectPool.h"

struct TLSFSpanDebugInfo
{
	enum class State
	{
		FREE,
		USED,
		WASTED
	};
	uint32_t m_offset;
	uint32_t m_size;
	State m_state;
};

class TLSFAllocator
{
public:
	explicit TLSFAllocator(uint32_t memorySize, uint32_t pageSize);
	bool alloc(uint32_t size, uint32_t alignment, uint32_t &spanOffset, void *&backingSpan);
	void free(void *backingSpan);
	uint32_t getAllocationCount() const;
	void getDebugInfo(size_t *count, TLSFSpanDebugInfo *info) const;
	uint32_t getMemorySize() const;
	uint32_t getPageSize() const;
	void getFreeUsedWastedSizes(uint32_t &free, uint32_t &used, uint32_t &wasted) const;

private:
	enum
	{
		MAX_FIRST_LEVELS = 32,
		MAX_LOG2_SECOND_LEVELS = 5,
		MAX_SECOND_LEVELS = 1 << MAX_LOG2_SECOND_LEVELS,
		SMALL_BLOCK = MAX_FIRST_LEVELS
	};

	struct Span
	{
		Span *m_previous;
		Span *m_next;
		Span *m_previousPhysical;
		Span *m_nextPhysical;
		uint32_t m_offset;
		uint32_t m_size;
		uint32_t m_usedOffset;
		uint32_t m_usedSize;
	};

	const uint32_t m_memorySize;
	const uint32_t m_pageSize;
	uint32_t m_firstLevelBitset;
	uint32_t m_secondLevelBitsets[MAX_FIRST_LEVELS];
	uint32_t m_smallBitset;
	Span *m_freeSpans[MAX_FIRST_LEVELS][MAX_SECOND_LEVELS];
	Span *m_smallFreeSpans[32];
	Span *m_firstPhysicalSpan;
	uint32_t m_allocationCount;
	uint32_t m_freeSize;
	uint32_t m_usedSize;
	uint32_t m_requiredDebugSpanCount;
	DynamicObjectPool<Span> m_spanPool;

	// returns indices of the list holding memory blocks that lie in the same size class as requestedSize.
	// used for inserting free blocks into the data structure
	void mappingInsert(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex);
	// returns indices of the list holding memory blocks that are one size class above requestedSize,
	// ensuring, that any block in the list can be used. used for alloc
	void mappingSearch(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex);
	bool findFreeSpan(uint32_t &firstLevelIndex, uint32_t &secondLevelIndex);
	Span *findFreeSpan(uint32_t size);
	void addSpanToFreeList(Span *span);
	void removeSpanFromFreeList(Span *span);
	void checkIntegrity();
};