#pragma once
#include <stdint.h>
#include "DeletedCopyMove.h"

class Thread
{
public:
	using EntryFunction = void (*)(void *arg);

	/// <summary>
	/// On Windows, this returns a pseudo handle that refers to the current thread. Using this handle
	/// on another thread will refer to that thread instead.
	/// </summary>
	/// <returns></returns>
	static void *getCurrentThreadHandle() noexcept;
	static uint64_t getHardwareThreadCount() noexcept;
	static void yield() noexcept;
	static void sleep(uint64_t milliseconds) noexcept;
	static bool setCoreAffinity(void *threadHandle, size_t coreAffinity) noexcept;

	explicit Thread() noexcept = default;
	explicit Thread(EntryFunction entryFunction, void *arg, size_t stackSize, const char *name) noexcept;
	DELETED_COPY(Thread);
	Thread(Thread &&other) noexcept;
	Thread &operator=(Thread &&other) noexcept;
	~Thread() noexcept;
	bool isValid() const noexcept;
	void *getHandle() noexcept;
	bool join() noexcept;
	void detach() noexcept;

private:
	void *m_threadHandle = nullptr;
};