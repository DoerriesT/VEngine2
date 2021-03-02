#include "Engine.h"
#include <EASTL/fixed_vector.h>
#include "task/Task.h"
#include <stdio.h>

void *__cdecl operator new[](size_t size, const char *name, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

void *__cdecl operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char *pName, int flags, unsigned debugFlags, const char *file, int line)
{
	return new uint8_t[size];
}

int main(int argc, char *argv[])
{
	eastl::fixed_vector<int, 10> v;

	for (int i = 0; i < 10; ++i)
	{
		v.push_back(i);
	}

	task::init();

	auto *waitGroup = task::allocWaitGroup();
	
	task::Task tasks[256];
	for (size_t i = 0; i < 256; ++i)
	{
		tasks[i] = task::Task([](void *param)
			{
				int taskIdx = (int)(size_t)param;
				printf("Task %d says hello from thread %d and fiber %d\n", taskIdx, (int)task::getThreadIndex(), (int)task::getFiberIndex());
			}, (void *)i);
	}
	
	task::schedule(256, tasks, waitGroup, task::Priority::NORMAL);
	
	task::waitFor(waitGroup);

	task::shutdown();

	Engine engine;
	return engine.start(argc, argv);
}