#include "DefaultAllocator.h"
#include <stdlib.h>
#include "profiling/Profiling.h"

DefaultAllocator *DefaultAllocator::get() noexcept
{
	static DefaultAllocator instance;
	return &instance;
}

void *DefaultAllocator::allocate(size_t n, int flags) noexcept
{
	void *ptr = malloc(n);
	PROFILING_MEM_ALLOC(ptr, n);
	return ptr;
}

void *DefaultAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	void *ptr = malloc(n);
	PROFILING_MEM_ALLOC(ptr, n);
	return ptr;
}

void DefaultAllocator::deallocate(void *p, size_t n) noexcept
{
	PROFILING_MEM_FREE(p);
	free(p);
}

const char *DefaultAllocator::get_name() const noexcept
{
	return m_name;
}

void DefaultAllocator::set_name(const char *pName) noexcept
{
	m_name = pName;
}

void *operator new(std::size_t count)
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new[](std::size_t count)
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new(std::size_t count, std::align_val_t al)
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new[](std::size_t count, std::align_val_t al)
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new(std::size_t count, const std::nothrow_t &) noexcept
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new[](std::size_t count, const std::nothrow_t &) noexcept
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new(std::size_t count, std::align_val_t al, const std::nothrow_t &) noexcept
{
	return DefaultAllocator::get()->allocate(count);
}

void *operator new[](std::size_t count, std::align_val_t al, const std::nothrow_t &) noexcept
{
	return DefaultAllocator::get()->allocate(count);
}

void operator delete(void *ptr) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete[](void *ptr) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete(void *ptr, std::align_val_t al) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete[](void *ptr, std::align_val_t al) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete  (void *ptr, std::size_t sz) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete[](void *ptr, std::size_t sz) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete  (void *ptr, std::size_t sz, std::align_val_t al) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}

void operator delete[](void *ptr, std::size_t sz, std::align_val_t al) noexcept
{
	DefaultAllocator::get()->deallocate(ptr, 0);
}
