#pragma once
#include <stdint.h>

namespace task
{
	struct WaitGroup;
	typedef void EntryPoint(void *param);

	enum class Priority
	{
		LOW,
		NORMAL,
		HIGH,
		CRITICAL
	};

	void init();
	void shutdown();
	WaitGroup *allocWaitGroup();
	void freeWaitGroup(WaitGroup *waitGroup);

	struct Task
	{
		EntryPoint *m_entryPoint = nullptr;
		void *m_param = nullptr;
		WaitGroup *m_waitGroup = nullptr;

		Task() = default;

		explicit inline Task(EntryPoint *entryPoint, void *param)
			:m_entryPoint(entryPoint),
			m_param(param)
		{

		}
	};

	void schedule(const Task &task, WaitGroup *waitGroup, Priority priority);
	void schedule(uint32_t count, Task *tasks, WaitGroup *waitGroup, Priority priority);

	void waitFor(WaitGroup *waitGroup);

	size_t getThreadIndex();
	size_t getFiberIndex();
}