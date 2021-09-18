#pragma once
#include <new>

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

void *operator new(std::size_t count, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, IAllocator *allocator) noexcept;
void *operator new(std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept;
void *operator new(std::size_t count, std::size_t al, IAllocator *allocator) noexcept;
void *operator new[](std::size_t count, std::size_t al, IAllocator *allocator) noexcept;
void operator delete(void *ptr, IAllocator *allocator) noexcept;
void operator delete[](void *ptr, IAllocator *allocator) noexcept;

#define ALLOC_NEW(allocator, type) new ((allocator)->allocate(sizeof(type), alignof(type), 0))
#define ALLOC_DELETE(allocator, ptr) (allocator)->deleteObject(ptr)