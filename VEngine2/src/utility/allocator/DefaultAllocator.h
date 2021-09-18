#pragma once
#include "IAllocator.h"

class DefaultAllocator : public IAllocator
{
public:
	static DefaultAllocator *get() noexcept;

	void *allocate(size_t n, int flags = 0) noexcept override;
	void *allocate(size_t n, size_t alignment, size_t offset, int flags = 0) noexcept override;
	void  deallocate(void *p, size_t n) noexcept override;
	const char *get_name() const noexcept override;
	void set_name(const char *pName) noexcept override;

private:
	const char *m_name = "Default Allocator";
};