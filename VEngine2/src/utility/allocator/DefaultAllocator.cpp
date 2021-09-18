#include "DefaultAllocator.h"
#include <stdlib.h>

DefaultAllocator *DefaultAllocator::get() noexcept
{
	static DefaultAllocator instance;
	return &instance;
}

void *DefaultAllocator::allocate(size_t n, int flags) noexcept
{
	return malloc(n);
}

void *DefaultAllocator::allocate(size_t n, size_t alignment, size_t offset, int flags) noexcept
{
	return malloc(n);
}

void DefaultAllocator::deallocate(void *p, size_t n) noexcept
{
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
