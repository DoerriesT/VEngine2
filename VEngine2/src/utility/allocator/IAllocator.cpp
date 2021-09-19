#include "IAllocator.h"
#include <new>

void *operator new(size_t size, void *pObjMem, PlacementNewDummy dummy) noexcept
{
	assert(pObjMem);
	return pObjMem;
}

void operator delete(void *pObj, void *pObjMem, PlacementNewDummy dummy) noexcept
{
	assert(false);
}

void *operator new(std::size_t count, IAllocator *allocator) noexcept
{
	return allocator->allocate(count);
}

void *operator new[](std::size_t count, IAllocator *allocator) noexcept
{
	return allocator->allocate(count);
}

void *operator new(std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept
{
	return allocator->allocate(count, static_cast<size_t>(al), 0);
}

void *operator new[](std::size_t count, std::align_val_t al, IAllocator *allocator) noexcept
{
	return allocator->allocate(count, static_cast<size_t>(al), 0);
}

void *operator new(std::size_t count, std::size_t al, IAllocator *allocator) noexcept
{
	return allocator->allocate(count, al, 0);
}

void *operator new[](std::size_t count, std::size_t al, IAllocator *allocator) noexcept
{
	return allocator->allocate(count, al, 0);
}

void operator delete(void *ptr, IAllocator *allocator) noexcept
{
	allocator->deallocate(ptr, 0);
}

void operator delete[](void *ptr, IAllocator *allocator) noexcept
{
	allocator->deallocate(ptr, 0);
}