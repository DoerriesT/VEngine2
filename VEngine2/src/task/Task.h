#pragma once
#include <stdint.h>

namespace task
{
	struct Counter;
	typedef void EntryPoint(void *param);

	enum class Priority
	{
		LOW,
		NORMAL,
		HIGH,
	};

	struct Task
	{
		EntryPoint *m_entryPoint = nullptr;
		void *m_param = nullptr;
		const char *m_name = "Unnamed Task";
		Counter *m_counter = nullptr;

		Task() = default;

		explicit inline Task(EntryPoint *entryPoint, void *param, const char *name) noexcept
			:m_entryPoint(entryPoint),
			m_param(param),
			m_name(name)
		{

		}
	};

	void init() noexcept;
	void shutdown() noexcept;
	void run(size_t count, Task *tasks, Counter **counter, Priority priority = Priority::NORMAL) noexcept;
	void waitForCounter(Counter *counter, bool stayOnThread = true) noexcept;
	void freeCounter(Counter *counter) noexcept;
	__declspec(noinline) size_t getThreadIndex() noexcept;
	size_t getFiberIndex() noexcept;
	size_t getThreadCount() noexcept;
	bool isManagedThread() noexcept;
}