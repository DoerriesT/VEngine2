#pragma once
#include <EASTL/vector.h>
#include <stdint.h>

/// <summary>
/// Utility class for creating and freeing uint32 handles.
/// </summary>
class HandleManager
{
public:
	/// <summary>
	/// Constructs a new HandleManager.
	/// </summary>
	/// <param name="maxHandle">The maximum handle value. If all handle values lower or equal to this have been exhausted, allocation will fail.</param>
	explicit HandleManager(uint32_t maxHandle = UINT32_MAX) noexcept;

	/// <summary>
	/// Allocate a handle.
	/// </summary>
	/// <param name="transient">If true, the handle cannot be freed individually, but only in bulk with all other transient handles.</param>
	/// <returns>A new handle or a null handle if allocation failed.</returns>
	uint32_t allocate(bool transient = false) noexcept;

	/// <summary>
	/// Frees a previously allocated handle. The handle will be recycled for future allocations.
	/// </summary>
	/// <param name="handle">The handle to be freed. Passing in a null handle is valid.</param>
	void free(uint32_t handle) noexcept;

	/// <summary>
	/// Frees all transient handles.
	/// </summary>
	void freeTransientHandles() noexcept;

	/// <summary>
	/// Checks if a given handle is valid (i.e. was previously allocated and not yet freed).
	/// </summary>
	/// <param name="handle">The handle to check for validity.</param>
	/// <returns>True if the handle is valid.</returns>
	bool isValidHandle(uint32_t handle) const noexcept;

private:
	eastl::vector<uint32_t> m_freeHandles; // holds freed handles that will be recycled for future allocations.
	eastl::vector<uint32_t> m_transientHandles; // holds transient handles that are bulk-freed.
	uint32_t m_nextFreeHandle = 1; // 0 is reserved as null handle
	uint32_t m_maxHandle = UINT32_MAX;
};