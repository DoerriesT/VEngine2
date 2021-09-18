#pragma once

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

#define ALLOC_PLACE_NEW(allocator, type) new (alignof(type), allocator)
#define ALLOC_PLACE_DELETE(allocator, ptr) delete (allocator) ptr