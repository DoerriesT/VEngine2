#include "TLSFAllocator.h"
#include <memory>
#include <cassert>
#include "Utility.h"

TLSFAllocator::TLSFAllocator(uint32_t memorySize, uint32_t pageSize)
	:m_memorySize(memorySize),
	m_pageSize(pageSize),
	m_firstLevelBitset(),
	m_smallBitset(),
	m_firstPhysicalSpan(nullptr),
	m_allocationCount(),
	m_freeSize(memorySize),
	m_usedSize(),
	m_requiredDebugSpanCount(1),
	m_spanPool(256)
{
	memset(m_secondLevelBitsets, 0, sizeof(m_secondLevelBitsets));
	memset(m_freeSpans, 0, sizeof(m_freeSpans));
	memset(m_smallFreeSpans, 0, sizeof(m_smallFreeSpans));

	// add span of memory, spanning the whole block
	Span *span = m_spanPool.alloc();
	memset(span, 0, sizeof(Span));

	span->m_size = m_memorySize;
	m_firstPhysicalSpan = span;
	addSpanToFreeList(span);
}

bool TLSFAllocator::alloc(uint32_t size, uint32_t alignment, uint32_t &spanOffset, void *&backingSpan)
{
	assert(size > 0);

	Span *freeSpan = nullptr;
	uint32_t alignedOffset = 0;

	// first try without adding (alignment - 1) and if that does not succeed add (alignment - 1)
	// to guarantee that the aligned offset + requested size fit into the allocated span
	for (int i = 0; i < 2; ++i)
	{
		Span *span = findFreeSpan(i == 0 ? size : size + alignment - 1);

		// no free span of requested size found
		if (!span)
		{
			return false;
		}

		// offset into this span where alignment requirement is satisfied
		alignedOffset = util::alignUp(span->m_offset, alignment);

		assert(i == 0 || alignedOffset + size <= span->m_offset + span->m_size);

		// test alignment
		if (alignedOffset + size <= span->m_offset + span->m_size)
		{
			freeSpan = span;
			break;
		}
	}

	if (!freeSpan)
	{
		return false;
	}

	assert(freeSpan);
	assert(freeSpan->m_size >= size);
	assert(!freeSpan->m_previous);
	assert(!freeSpan->m_previousPhysical || freeSpan->m_previousPhysical->m_offset + freeSpan->m_previousPhysical->m_size == freeSpan->m_offset);
	assert(!freeSpan->m_nextPhysical || freeSpan->m_offset + freeSpan->m_size == freeSpan->m_nextPhysical->m_offset);

	removeSpanFromFreeList(freeSpan);

	uint32_t nextLowerPageSizeOffset = util::alignDown(alignedOffset, m_pageSize);
	assert(nextLowerPageSizeOffset <= alignedOffset);
	assert(nextLowerPageSizeOffset >= freeSpan->m_offset);

	uint32_t nextUpperPageSizeOffset = util::alignUp(alignedOffset + size, m_pageSize);
	assert(nextUpperPageSizeOffset >= alignedOffset + size);
	assert(nextUpperPageSizeOffset <= freeSpan->m_offset + freeSpan->m_size);

	// all spans must start at a multiple of page size, so calculate margin from next lower multiple, not from actual used offset
	const uint32_t beginMargin = nextLowerPageSizeOffset - freeSpan->m_offset;
	// since all spans start at a multiple of page size, they must also end at such a multiple, so calculate margin from next upper
	// multiple to end of span
	const uint32_t endMargin = freeSpan->m_offset + freeSpan->m_size - nextUpperPageSizeOffset;

	// split span at beginning?
	if (beginMargin >= m_pageSize)
	{
		Span *beginSpan = m_spanPool.alloc();
		memset(beginSpan, 0, sizeof(Span));

		beginSpan->m_previousPhysical = freeSpan->m_previousPhysical;
		beginSpan->m_nextPhysical = freeSpan;
		beginSpan->m_offset = freeSpan->m_offset;
		beginSpan->m_size = beginMargin;

		if (freeSpan->m_previousPhysical)
		{
			assert(freeSpan->m_previousPhysical->m_nextPhysical == freeSpan);
			freeSpan->m_previousPhysical->m_nextPhysical = beginSpan;
		}
		else
		{
			m_firstPhysicalSpan = beginSpan;
		}

		freeSpan->m_offset += beginMargin;
		freeSpan->m_size -= beginMargin;
		freeSpan->m_previousPhysical = beginSpan;

		// add begin span to free list
		addSpanToFreeList(beginSpan);
		++m_requiredDebugSpanCount;
	}

	// split span at end?
	if (endMargin >= m_pageSize)
	{
		Span *endSpan = m_spanPool.alloc();
		memset(endSpan, 0, sizeof(Span));

		endSpan->m_previousPhysical = freeSpan;
		endSpan->m_nextPhysical = freeSpan->m_nextPhysical;
		endSpan->m_offset = nextUpperPageSizeOffset;
		endSpan->m_size = endMargin;

		if (freeSpan->m_nextPhysical)
		{
			assert(freeSpan->m_nextPhysical->m_previousPhysical == freeSpan);
			freeSpan->m_nextPhysical->m_previousPhysical = endSpan;
		}

		freeSpan->m_nextPhysical = endSpan;
		freeSpan->m_size -= endMargin;

		// add end span to free list
		addSpanToFreeList(endSpan);
		++m_requiredDebugSpanCount;
	}

	++m_allocationCount;

	// fill out allocation info
	spanOffset = alignedOffset;
	backingSpan = freeSpan;

	// update span usage
	freeSpan->m_usedOffset = alignedOffset;
	freeSpan->m_usedSize = size;

	m_freeSize -= freeSpan->m_size;
	m_usedSize += freeSpan->m_usedSize;
	m_requiredDebugSpanCount += freeSpan->m_usedOffset > freeSpan->m_offset ? 1 : 0;
	m_requiredDebugSpanCount += (freeSpan->m_offset + freeSpan->m_size) > (freeSpan->m_usedOffset + freeSpan->m_usedSize) ? 1 : 0;

#ifdef _DEBUG
	checkIntegrity();
#endif // _DEBUG

	return true;
}

void TLSFAllocator::free(void *backingSpan)
{
	Span *span = reinterpret_cast<Span *>(backingSpan);
	assert(!span->m_next);
	assert(!span->m_previous);

	m_freeSize += span->m_size;
	m_usedSize -= span->m_usedSize;
	m_requiredDebugSpanCount -= span->m_usedOffset > span->m_offset ? 1 : 0;
	m_requiredDebugSpanCount -= (span->m_offset + span->m_size) > (span->m_usedOffset + span->m_usedSize) ? 1 : 0;

	// is next physical span also free? -> merge
	{
		Span *nextPhysical = span->m_nextPhysical;
		if (nextPhysical && nextPhysical->m_usedSize == 0)
		{
			// remove next physical span from free list
			removeSpanFromFreeList(nextPhysical);

			// merge spans
			span->m_size += nextPhysical->m_size;
			span->m_nextPhysical = nextPhysical->m_nextPhysical;
			if (nextPhysical->m_nextPhysical)
			{
				nextPhysical->m_nextPhysical->m_previousPhysical = span;
			}

			m_spanPool.free(nextPhysical);
			--m_requiredDebugSpanCount;
		}
	}

	// is previous physical span also free? -> merge
	{
		Span *previousPhysical = span->m_previousPhysical;
		if (previousPhysical && previousPhysical->m_usedSize == 0)
		{
			// remove previous physical span from free list
			removeSpanFromFreeList(previousPhysical);

			// merge spans
			previousPhysical->m_size += span->m_size;
			previousPhysical->m_nextPhysical = span->m_nextPhysical;
			if (span->m_nextPhysical)
			{
				span->m_nextPhysical->m_previousPhysical = previousPhysical;
			}

			m_spanPool.free(span);

			span = previousPhysical;
			--m_requiredDebugSpanCount;
		}
	}

	// add span to free list
	{
		addSpanToFreeList(span);
	}

	--m_allocationCount;

#ifdef _DEBUG
	checkIntegrity();
#endif // _DEBUG
}

uint32_t TLSFAllocator::getAllocationCount() const
{
	return m_allocationCount;
}

void TLSFAllocator::getDebugInfo(size_t *count, TLSFSpanDebugInfo *info) const
{
	if (!info)
	{
		*count = m_requiredDebugSpanCount;
	}
	else
	{
		assert(*count >= m_requiredDebugSpanCount);
		uint32_t free = 0;
		uint32_t used = 0;
		uint32_t wasted = 0;
		Span *span = m_firstPhysicalSpan;
		size_t i = 0;
		while (span && i < *count)
		{
			// span is free
			if (span->m_usedSize == 0 && i < *count)
			{
				TLSFSpanDebugInfo &spanInfo = info[i++];
				spanInfo.m_offset = span->m_offset;
				spanInfo.m_size = span->m_size;
				spanInfo.m_state = TLSFSpanDebugInfo::State::FREE;
				free += spanInfo.m_size;
			}
			else
			{
				// wasted size at start of suballocation
				if (span->m_usedOffset > span->m_offset &&i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_offset;
					spanInfo.m_size = span->m_usedOffset - span->m_offset;
					spanInfo.m_state = TLSFSpanDebugInfo::State::WASTED;
					wasted += spanInfo.m_size;
				}

				// used space of suballocation
				if (i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_usedOffset;
					spanInfo.m_size = span->m_usedSize;
					spanInfo.m_state = TLSFSpanDebugInfo::State::USED;
					used += spanInfo.m_size;
				}

				// wasted space at end of suballocation
				if (span->m_offset + span->m_size > span->m_usedOffset + span->m_usedSize && i < *count)
				{
					TLSFSpanDebugInfo &spanInfo = info[i++];
					spanInfo.m_offset = span->m_usedOffset + span->m_usedSize;
					spanInfo.m_size = span->m_offset + span->m_size - span->m_usedOffset - span->m_usedSize;
					spanInfo.m_state = TLSFSpanDebugInfo::State::WASTED;
					wasted += spanInfo.m_size;
				}
			}

			span = span->m_nextPhysical;
		}
		assert(span == nullptr);
		assert(free == m_freeSize);
		assert(used == m_usedSize);
		assert(wasted == (m_memorySize - m_freeSize - m_usedSize));
		assert((free + used + wasted) == m_memorySize);
	}
}

uint32_t TLSFAllocator::getMemorySize() const
{
	return m_memorySize;
}

uint32_t TLSFAllocator::getPageSize() const
{
	return m_pageSize;
}

void TLSFAllocator::getFreeUsedWastedSizes(uint32_t &free, uint32_t &used, uint32_t &wasted) const
{
	free = m_freeSize;
	used = m_usedSize;
	wasted = m_memorySize - m_freeSize - m_usedSize;
}

void TLSFAllocator::mappingInsert(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	assert(size >= SMALL_BLOCK);
	
	firstLevelIndex = util::findLastSetBit(size);
	secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVELS)) - MAX_SECOND_LEVELS;

	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);
}

void TLSFAllocator::mappingSearch(uint32_t size, uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	assert(size >= SMALL_BLOCK);

	uint32_t t = (1 << (util::findLastSetBit(size) - MAX_LOG2_SECOND_LEVELS)) - 1;
	size += t;
	firstLevelIndex = util::findLastSetBit(size);
	secondLevelIndex = (size >> (firstLevelIndex - MAX_LOG2_SECOND_LEVELS)) - MAX_SECOND_LEVELS;
	size &= ~t;

	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);
}

bool TLSFAllocator::findFreeSpan(uint32_t &firstLevelIndex, uint32_t &secondLevelIndex)
{
	assert(firstLevelIndex < MAX_FIRST_LEVELS);
	assert(secondLevelIndex < MAX_SECOND_LEVELS);

	uint32_t bitsetTmp = m_secondLevelBitsets[firstLevelIndex] & (~0u << secondLevelIndex);

	if (bitsetTmp)
	{
		secondLevelIndex = util::findFirstSetBit(bitsetTmp);
		return true;
	}
	else
	{
		firstLevelIndex = util::findFirstSetBit(m_firstLevelBitset & (~0u << (firstLevelIndex + 1)));
		if (firstLevelIndex != UINT32_MAX)
		{
			secondLevelIndex = util::findFirstSetBit(m_secondLevelBitsets[firstLevelIndex]);
			return true;
		}
	}
	return false;
}

TLSFAllocator::Span *TLSFAllocator::findFreeSpan(uint32_t size)
{
	Span *result = nullptr;

	if (size < SMALL_BLOCK && m_smallBitset != 0)
	{
		const uint32_t index = util::findFirstSetBit(m_smallBitset & (~0u << size));
		if (index != UINT32_MAX)
		{
			result = m_smallFreeSpans[index];
		}
	}
	else
	{
		// size must be at least SMALL_BLOCK size if we want to use tlsf allocation
		size = size < SMALL_BLOCK ? SMALL_BLOCK : size;

		uint32_t firstLevelIndex = 0;
		uint32_t secondLevelIndex = 0;
		// rounds up requested size to next power of two and finds indices of a list containing spans of the requested size class
		mappingSearch(size, firstLevelIndex, secondLevelIndex);

		for (int i = 0; i < 2; ++i)
		{
			// one last attempt: check the first element in the next smaller bucket size.
			// tlsf always searches in a bucket that has spans that are at least as big as the requested size. however,
			// the next smaller bucket may also have spans that are big enough. this may happen if a new TLSFAllocator
			// was created and the first allocation tries to allocate the whole managed memory range.
			if (i != 0)
			{
				if (secondLevelIndex == 0)
				{
					secondLevelIndex = 31;
					--firstLevelIndex;
				}
				else
				{
					--secondLevelIndex;
				}
			}

			uint32_t tmpFirstLevelIndex = firstLevelIndex;
			uint32_t tmpSecondLevelIndex = secondLevelIndex;

			// finds a free span and updates indices to the indices of the actual free list of the span
			if (findFreeSpan(tmpFirstLevelIndex, tmpSecondLevelIndex))
			{
				// if we are in the second iteration, it is possible that we got a span that is not big enough
				Span *span = m_freeSpans[tmpFirstLevelIndex][tmpSecondLevelIndex];
				if (span->m_size >= size)
				{
					result = m_freeSpans[tmpFirstLevelIndex][tmpSecondLevelIndex];
					break;
				}
			}
		}
	}
	
	return result;
}

void TLSFAllocator::addSpanToFreeList(Span *span)
{
	Span **list = nullptr;

	// get pointer to beginning of the correct list of free spans and update bitsets
	if (span->m_size < SMALL_BLOCK)
	{
		list = &m_smallFreeSpans[span->m_size];

		// update bitset
		m_smallBitset |= 1 << span->m_size;
	}
	else
	{
		uint32_t firstLevelIndex = 0;
		uint32_t secondLevelIndex = 0;
		mappingInsert(span->m_size, firstLevelIndex, secondLevelIndex);
		
		assert(firstLevelIndex < MAX_FIRST_LEVELS);
		assert(secondLevelIndex < MAX_SECOND_LEVELS);

		list = &m_freeSpans[firstLevelIndex][secondLevelIndex];

		// update bitsets
		m_secondLevelBitsets[firstLevelIndex] |= 1 << secondLevelIndex;
		m_firstLevelBitset |= 1 << firstLevelIndex;
	}

	// set span as new head
	Span *prevHead = *list;
	*list = span;

	// link span and previous head and mark span as free
	span->m_previous = nullptr;
	span->m_next = prevHead;
	span->m_usedOffset = 0;
	span->m_usedSize = 0;
	if (prevHead)
	{
		assert(!prevHead->m_previous);
		prevHead->m_previous = span;
	}
}

void TLSFAllocator::removeSpanFromFreeList(Span *span)
{
	if (span->m_size < SMALL_BLOCK)
	{
		// is span head of list?
		if (!span->m_previous)
		{
			assert(span == m_smallFreeSpans[span->m_size]);

			m_smallFreeSpans[span->m_size] = span->m_next;
			if (span->m_next)
			{
				span->m_next->m_previous = nullptr;
			}
			else
			{
				// update bitset, since list is now empty
				m_smallBitset &= ~(1 << span->m_size);
			}
		}
		else
		{
			span->m_previous->m_next = span->m_next;
			if (span->m_next)
			{
				span->m_next->m_previous = span->m_previous;
			}
		}
	}
	else
	{
		uint32_t firstLevelIndex = 0;
		uint32_t secondLevelIndex = 0;
		mappingInsert(span->m_size, firstLevelIndex, secondLevelIndex);
		
		// remove from free list
		{
			assert(firstLevelIndex < MAX_FIRST_LEVELS);
			assert(secondLevelIndex < MAX_SECOND_LEVELS);

			// is span head of list?
			if (!span->m_previous)
			{
				assert(span == m_freeSpans[firstLevelIndex][secondLevelIndex]);

				m_freeSpans[firstLevelIndex][secondLevelIndex] = span->m_next;
				if (span->m_next)
				{
					span->m_next->m_previous = nullptr;
				}
				else
				{
					// update bitsets, since list is now empty
					m_secondLevelBitsets[firstLevelIndex] &= ~(1 << secondLevelIndex);
					if (m_secondLevelBitsets[firstLevelIndex] == 0)
					{
						m_firstLevelBitset &= ~(1 << firstLevelIndex);
					}
				}
			}
			else
			{
				span->m_previous->m_next = span->m_next;
				if (span->m_next)
				{
					span->m_next->m_previous = span->m_previous;
				}
			}
		}
	}

	// remove previous and next pointers in span and mark it as used
	span->m_next = nullptr;
	span->m_previous = nullptr;
	span->m_usedOffset = span->m_offset;
	span->m_usedSize = span->m_size;
}

void TLSFAllocator::checkIntegrity()
{
	Span *span = m_firstPhysicalSpan;
	uint32_t currentOffset = 0;
	uint32_t free = 0;
	uint32_t used = 0;
	uint32_t wasted = 0;
	while (span)
	{
		assert(span->m_offset == currentOffset);
		currentOffset += span->m_size;
		free += span->m_usedSize == 0 ? span->m_size : 0;
		used += span->m_usedSize;
		wasted += span->m_usedSize == 0 ? 0 : span->m_size - span->m_usedSize;
		span = span->m_nextPhysical;
	}
	assert(currentOffset == m_memorySize);
	assert(free == m_freeSize);
	assert(used == m_usedSize);
	assert(wasted == (m_memorySize - m_freeSize - m_usedSize));
	assert((free + used + wasted) == m_memorySize);
}
