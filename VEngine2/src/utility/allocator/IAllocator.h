#pragma once
#include <new>
#include "utility/Memory.h"
#include "utility/DeletedCopyMove.h"
#include <assert.h>

struct PlacementNewDummy 
{
	explicit PlacementNewDummy() {};
};

// Overrides for new/delete with a dummy parameter to ensure there won't be a conflict if a library user
// tries to override global placement new.
void *operator new(size_t size, void *pObjMem, PlacementNewDummy dummy) noexcept;

// Override for delete should never be called, this is only here to make sure there are no compiler warnings.
void operator delete(void *pObj, void *pObjMem, PlacementNewDummy dummy) noexcept;

#define PLACEMENT_NEW(memory) new ((memory), PlacementNewDummy{})
#define ALLOC_NEW(allocator, type) PLACEMENT_NEW((allocator)->allocate(sizeof(type), alignof(type), 0)) type
#define ALLOC_DELETE(allocator, ptr) (allocator)->deleteObject(ptr)

/// <summary>
/// Generic allocator interface for passing around allocators. Uses the EASTL allocator template interface.
/// </summary>
class IAllocator
{
public:
	// EASTL allocator interface:

	virtual void *allocate(size_t n, int flags = 0) noexcept = 0;
	virtual void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept = 0;
	virtual void  deallocate(void *p, size_t n) noexcept = 0;
	virtual const char *get_name() const noexcept = 0;
	virtual void set_name(const char *pName) noexcept = 0;
	virtual ~IAllocator() noexcept = default;

	template<typename T>
	T *allocateArray(size_t count) noexcept
	{
		return reinterpret_cast<T *>(allocate(count * sizeof(T), alignof(T), 0));
	}

	template<typename T>
	void deleteObject(T *obj) noexcept
	{
		if (obj)
		{
			obj->~T();
			deallocate(obj, sizeof(T));
		}
	}
};

class ProxyAllocator
{
public:
	explicit ProxyAllocator(IAllocator *allocator)
		:m_allocator(allocator)
	{

	}

	void *allocate(size_t n, int flags = 0) noexcept
	{
		return m_allocator->allocate(n, flags);
	}

	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept
	{
		return m_allocator->allocate(n, alignment, offset, flags);
	}

	void  deallocate(void *p, size_t n) noexcept
	{
		return m_allocator->deallocate(p, n);
	}

	const char *get_name() const noexcept
	{
		return m_allocator->get_name();
	}

	void set_name(const char *pName) noexcept
	{
		m_allocator->set_name(pName);
	}

	bool operator==(const ProxyAllocator &other)
	{
		return other.m_allocator == m_allocator;
	}

private:
	IAllocator *m_allocator;
};

class ScopedAllocation
{
public:
	explicit ScopedAllocation(IAllocator *allocator, void *allocation, size_t allocationSize = 0) noexcept
		:m_allocator(allocator),
		m_allocation(allocation),
		m_allocationSize(allocationSize)
	{
		assert(allocation);
	}

	DELETED_COPY_MOVE(ScopedAllocation);

	~ScopedAllocation() noexcept
	{
		m_allocator->deallocate(m_allocation, m_allocationSize);
	}

	void *getAllocation() const noexcept
	{
		return m_allocation;
	}

	size_t getAllocationSize() const noexcept
	{
		return m_allocationSize;
	}

private:
	IAllocator *m_allocator;
	void *m_allocation;
	size_t m_allocationSize;
};

template<typename T>
class UniquePtr
{
public:

	template<typename... Args>
	static UniquePtr create(IAllocator *allocator, Args &&... args) noexcept
	{
		return UniquePtr<T>(ALLOC_NEW(allocator, T) (static_cast<Args &&>(args)...), allocator);
	}

	UniquePtr() = default;
	
	explicit UniquePtr(T *ptr, IAllocator *allocator) noexcept
		:m_ptr(ptr),
		m_allocator(allocator)
	{
	}

	template<typename T2>
	UniquePtr(UniquePtr<T2> &&other) noexcept
		:m_ptr(static_cast<T *>(other.m_ptr)),
		m_allocator(other.m_allocator)
	{
		other.m_ptr = nullptr;
		other.m_allocator = nullptr;
	}

	template<typename T2>
	UniquePtr &operator=(UniquePtr<T2> &&other) noexcept
	{
		if (&other != this)
		{
			if (m_ptr)
			{
				ALLOC_DELETE(m_allocator, m_ptr);
			}
			m_ptr = static_cast<T *>(other.m_ptr);
			m_allocator = other.m_allocator;

			other.m_ptr = nullptr;
			other.m_allocator = nullptr;
		}
		
		return *this;
	}

	DELETED_COPY(UniquePtr);

	~UniquePtr() noexcept
	{
		ALLOC_DELETE(m_allocator, m_ptr);
	}

	void reset() noexcept
	{
		if (m_ptr)
		{
			ALLOC_DELETE(m_allocator, m_ptr);
		}
		m_ptr = nullptr;
		m_allocator = nullptr;
	}

	T *get() const noexcept
	{
		return m_ptr;
	}

	IAllocator *getAllocator() const noexcept
	{
		return m_allocator;
	}

	T &operator*() const noexcept
	{
		return *m_ptr;
	}

	T *operator->() const noexcept
	{
		return m_ptr;
	}

private:
	T *m_ptr = nullptr;
	IAllocator *m_allocator = nullptr;
};

void *operator new(std::size_t count, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, IAllocator *allocator) noexcept;
void *operator new(std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept;
void *operator new(std::size_t count, std::size_t al, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, std::size_t al, IAllocator *allocator) noexcept;
void operator delete(void *ptr, IAllocator *allocator) noexcept;
void operator delete[](void *ptr, IAllocator *allocator) noexcept;