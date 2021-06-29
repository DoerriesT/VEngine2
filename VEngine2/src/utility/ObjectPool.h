#pragma once
#include <vector>
#include <cassert>

template<typename T>
struct RawView
{
	alignas(alignof(T)) char m_data[sizeof(T)];
};

template<typename T, size_t Count>
class StaticObjectPool
{
public:
	using ObjectType = T;

	explicit StaticObjectPool();
	T *alloc();
	void free(T *value);
	size_t getAllocationCount() const;

private:
	union Item
	{
		size_t m_nextFreeItem;
		T m_value;
	};

	Item m_items[Count];
	size_t m_firstFreeIndex;
	size_t m_allocationCount;
};

template<typename T, size_t Count>
using StaticObjectMemoryPool = StaticObjectPool<RawView<T>, Count>;

template<typename T, size_t Count>
inline StaticObjectPool<T, Count>::StaticObjectPool()
	:m_firstFreeIndex(),
	m_allocationCount()
{
	// setup linked list
	for (size_t i = 0; i < Count; ++i)
	{
		m_items[i].m_nextFreeItem = i + 1;
	}

	m_items[Count - 1].m_nextFreeItem = ~size_t(0);
}

template<typename T, size_t Count>
inline T *StaticObjectPool<T, Count>::alloc()
{
	if (m_firstFreeIndex == ~size_t(0))
	{
		return nullptr;
	}

	++m_allocationCount;

	Item &item = m_items[m_firstFreeIndex];
	m_firstFreeIndex = item.m_nextFreeItem;
	return &item.m_value;
}

template<typename T, size_t Count>
inline void StaticObjectPool<T, Count>::free(T *value)
{
	Item *item = reinterpret_cast<Item *>(value);
	size_t index = item - m_items;
	item->m_nextFreeItem = m_firstFreeIndex;
	m_firstFreeIndex = index;

	--m_allocationCount;
}

template<typename T, size_t Count>
inline size_t StaticObjectPool<T, Count>::getAllocationCount() const
{
	return m_allocationCount;
}

template<typename T>
class DynamicObjectPool
{
public:
	using ObjectType = T;

	explicit DynamicObjectPool(size_t firstBlockCapacity);
	DynamicObjectPool(DynamicObjectPool &) = delete;
	DynamicObjectPool(DynamicObjectPool &&) = delete;
	DynamicObjectPool &operator= (const DynamicObjectPool &) = delete;
	DynamicObjectPool &operator= (const DynamicObjectPool &&) = delete;
	~DynamicObjectPool();
	T *alloc();
	void free(T *value);
	void clear();
	size_t getAllocationCount() const;

private:
	union Item
	{
		size_t m_nextFreeItem;
		T m_value;
	};

	struct ItemBlock
	{
		Item *m_items;
		size_t m_firstFreeIndex;
		size_t m_capacity;
	};

	const size_t m_firstBlockCapacity;
	size_t m_allocationCount;
	std::vector<ItemBlock> m_blocks;

	void createBlock();
};

template<typename T>
using DynamicObjectMemoryPool = DynamicObjectPool<RawView<T>>;

template<typename T>
inline DynamicObjectPool<T>::DynamicObjectPool(size_t firstBlockCapacity)
	:m_firstBlockCapacity(firstBlockCapacity),
	m_allocationCount(),
	m_blocks()
{
	assert(m_firstBlockCapacity > 1);
}

template<typename T>
inline DynamicObjectPool<T>::~DynamicObjectPool()
{
	clear();
}

template<typename T>
inline T *DynamicObjectPool<T>::alloc()
{
	for (size_t i = m_blocks.size(); i--;)
	{
		ItemBlock &block = m_blocks[i];

		// this block has free items, use the first
		if (block.m_firstFreeIndex != ~size_t(0))
		{
			Item &item = block.m_items[block.m_firstFreeIndex];
			block.m_firstFreeIndex = item.m_nextFreeItem;

			++m_allocationCount;

			return &item.m_value;
		}
	}

	// no block has free items, create a new block
	createBlock();
	ItemBlock &block = m_blocks.back();
	Item &item = block.m_items[block.m_firstFreeIndex];
	block.m_firstFreeIndex = item.m_nextFreeItem;

	++m_allocationCount;

	return &item.m_value;
}

template<typename T>
inline void DynamicObjectPool<T>::free(T *value)
{
	Item *item = reinterpret_cast<Item *>(value);

	// search blocks to find ptr
	for (size_t i = m_blocks.size(); i--;)
	{
		ItemBlock &block = m_blocks[i];

		if ((item >= block.m_items) && (item < (block.m_items + block.m_capacity)))
		{
			size_t index = item - block.m_items;
			item->m_nextFreeItem = block.m_firstFreeIndex;
			block.m_firstFreeIndex = index;

			--m_allocationCount;

			return;
		}
	}

	assert(false);
}

template<typename T>
inline void DynamicObjectPool<T>::clear()
{
	for (size_t i = m_blocks.size(); i--; )
	{
		delete[] m_blocks[i].m_items;
	}

	m_blocks.clear();
}

template<typename T>
inline size_t DynamicObjectPool<T>::getAllocationCount() const
{
	return m_allocationCount;
}

template<typename T>
inline void DynamicObjectPool<T>::createBlock()
{
	const size_t capacity = m_blocks.empty() ? m_firstBlockCapacity : m_blocks.back().m_capacity * 3 / 2;

	ItemBlock block{};
	block.m_items = new Item[capacity];
	block.m_firstFreeIndex = 0;
	block.m_capacity = capacity;

	assert(block.m_items);

	m_blocks.push_back(block);

	// setup linked list
	for (size_t i = 0; i < capacity; ++i)
	{
		block.m_items[i].m_nextFreeItem = i + 1;
	}

	block.m_items[capacity - 1].m_nextFreeItem = ~size_t(0);
}