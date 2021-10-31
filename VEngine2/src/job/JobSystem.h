#pragma once
#include <stdint.h>

namespace job
{
	struct Counter;
	typedef void EntryPoint(void *param);

	enum class Priority
	{
		LOW,
		NORMAL,
		HIGH,
	};

	struct Job
	{
		EntryPoint *m_entryPoint = nullptr;
		void *m_param = nullptr;
		Counter *m_counter = nullptr;

		Job() = default;
		explicit inline Job(EntryPoint *entryPoint, void *param) noexcept
			:m_entryPoint(entryPoint),
			m_param(param)
		{
		}
	};

	void init() noexcept;
	void shutdown() noexcept;
	void run(size_t count, Job *jobs, Counter **counter, Priority priority = Priority::NORMAL) noexcept;
	void waitForCounter(Counter *counter, bool stayOnThread = true) noexcept;
	void freeCounter(Counter *counter) noexcept;
	__declspec(noinline) size_t getThreadIndex() noexcept;
	size_t getFiberIndex() noexcept;
	size_t getThreadCount() noexcept;
	bool isManagedThread() noexcept;
}