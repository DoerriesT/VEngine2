#include "Task.h"
#include "Fiber.h"
#include <EASTL/array.h>
#include <EASTL/queue.h>
#include <EASTL/vector.h>
#include <thread>
#include <mutex>
#include <EASTL/atomic.h>
#include <concurrentqueue.h>

namespace
{
	struct PerThreadData
	{
		task::Fiber *m_threadFiber;
		task::Fiber *m_currentFiber;
	};

	struct PerFiberData
	{
		task::Fiber *m_oldFiberToPutOnFreeList;
	};

	struct TaskSchedulerData
	{
		static constexpr size_t s_numFibers = 128;
		eastl::array<task::Fiber, s_numFibers> m_fibers;
		eastl::array<PerFiberData, s_numFibers> m_perFiberData;
		eastl::vector<PerThreadData> m_perThreadData;
		moodycamel::ConcurrentQueue<task::Task> m_taskQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_resumableTasksQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_freeFibersQueue;
		moodycamel::ConcurrentQueue<task::WaitGroup *> m_freeWaitGroups;
		eastl::atomic_flag m_stopped = false;
		eastl::atomic<uint32_t> m_runningThreadCount = 0;
	};
}

namespace task
{
	struct WaitGroup
	{
		std::mutex m_mutex;
		uint32_t m_counter = 0;
		eastl::vector<task::Fiber *> m_waitingFibers;
	};
}

static TaskSchedulerData *s_taskSchedulerData = nullptr;
static thread_local size_t s_threadIndex = -1;
static thread_local bool s_isMainThread = false;

void task::init()
{
	assert(!s_taskSchedulerData);
	s_taskSchedulerData = new TaskSchedulerData();
	s_isMainThread = true;

	unsigned int numCores = std::thread::hardware_concurrency();
	numCores = numCores == 0 ? 4 : numCores;

	s_taskSchedulerData->m_perThreadData.resize(numCores);

	// create fibers
	for (size_t i = 0; i < TaskSchedulerData::s_numFibers; ++i)
	{
		auto fiberFunc = [](void *fiberParam)
		{
			TaskSchedulerData &schedulerData = *s_taskSchedulerData;
			size_t fiberIndex = (size_t)fiberParam;
			Fiber *self = &schedulerData.m_fibers[fiberIndex];

			while (!schedulerData.m_stopped.test())
			{
				// try to resume another task
				{
					Fiber *resumeFiber = nullptr;
					while (schedulerData.m_resumableTasksQueue.try_dequeue(resumeFiber))
					{
						// mark current fiber to be freed
						schedulerData.m_perFiberData[(size_t)resumeFiber->getFiberData()].m_oldFiberToPutOnFreeList = self;

						// switch to new fiber
						schedulerData.m_perThreadData[s_threadIndex].m_currentFiber = resumeFiber;
						self->switchToFiber(*resumeFiber);
					}
				}

				// no other tasks to resume -> fetch a fresh one
				Task curTask;

				// try to get a task from the queue
				if (schedulerData.m_taskQueue.try_dequeue(curTask))
				{
					// execute task
					curTask.m_entryPoint(curTask.m_param);

					// lock wait group, decrement and enqueue resumable tasks if counter hit 0
					{
						std::lock_guard<std::mutex> lock(curTask.m_waitGroup->m_mutex);

						// decrement
						--curTask.m_waitGroup->m_counter;

						// put all waiting fibers on resumable list
						if (curTask.m_waitGroup->m_counter == 0)
						{
							size_t waitingCount = curTask.m_waitGroup->m_waitingFibers.size();
							if (waitingCount != 0)
							{
								schedulerData.m_resumableTasksQueue.enqueue_bulk(curTask.m_waitGroup->m_waitingFibers.data(), waitingCount);
							}
							curTask.m_waitGroup->m_waitingFibers.clear();
						}
					}
				}
				else
				{
					eastl::cpu_pause();
				}
			}

			// switch back to original fiber
			Fiber *threadFiber = schedulerData.m_perThreadData[s_threadIndex].m_threadFiber;
			s_taskSchedulerData->m_perThreadData[s_threadIndex].m_currentFiber = threadFiber;
			self->switchToFiber(*threadFiber);
		};

		s_taskSchedulerData->m_fibers[i] = Fiber(fiberFunc, (void *)i);
		s_taskSchedulerData->m_freeFibersQueue.enqueue(&s_taskSchedulerData->m_fibers[i]);
	}

	// initialize thread counter. this is later used for shutting down the system
	s_taskSchedulerData->m_runningThreadCount = numCores;

	// create threads
	for (size_t i = 0; i < numCores; ++i)
	{
		std::thread([](size_t threadIdx)
			{
				s_threadIndex = threadIdx;

				// convert thread to fiber to be able to run other fibers
				Fiber threadFiber = Fiber::convertThreadToFiber((void*)(50 + threadIdx));
				s_taskSchedulerData->m_perThreadData[threadIdx].m_threadFiber = &threadFiber;

				// fetch a free fiber...
				Fiber *fiber = nullptr;
				while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
				{
				}

				// ... and switch to it: the fiber has its own loop
				s_taskSchedulerData->m_perThreadData[threadIdx].m_currentFiber = fiber;
				threadFiber.switchToFiber(*fiber);

				// decrement the thread counter to tell the system that this thread has finished
				--(s_taskSchedulerData->m_runningThreadCount);
			}, i).detach();
	}
}

void task::shutdown()
{
	assert(s_taskSchedulerData);
	assert(s_isMainThread);
	s_taskSchedulerData->m_stopped.test_and_set();

	// wait for all workers to finish
	while (s_taskSchedulerData->m_runningThreadCount != 0)
	{
		eastl::cpu_pause();
	}

	// delete all wait groups
	WaitGroup *waitGroup = nullptr;
	while (s_taskSchedulerData->m_freeWaitGroups.try_dequeue(waitGroup))
	{
		delete waitGroup;
	}

	delete s_taskSchedulerData;
	s_taskSchedulerData = nullptr;
}

task::WaitGroup *task::allocWaitGroup()
{
	WaitGroup *waitGroup = nullptr;
	if (!s_taskSchedulerData->m_freeWaitGroups.try_dequeue(waitGroup))
	{
		waitGroup = new WaitGroup();
	}
	return waitGroup;
}

void task::freeWaitGroup(WaitGroup *waitGroup)
{
	assert(waitGroup->m_counter == 0 && waitGroup->m_waitingFibers.size() == 0);
	s_taskSchedulerData->m_freeWaitGroups.enqueue(waitGroup);
}

void task::schedule(const Task &task, WaitGroup *waitGroup, Priority priority)
{
	//std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

	waitGroup->m_counter = 1;

	Task taskCopy = task;
	taskCopy.m_waitGroup = waitGroup;

	s_taskSchedulerData->m_taskQueue.enqueue(taskCopy);
}

void task::schedule(uint32_t count, Task *tasks, WaitGroup *waitGroup, Priority priority)
{
	//std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

	waitGroup->m_counter = count;

	for (size_t i = 0; i < count; ++i)
	{
		tasks[i].m_waitGroup = waitGroup;
	}

	s_taskSchedulerData->m_taskQueue.enqueue_bulk(tasks, count);
}

void task::waitFor(WaitGroup *waitGroup)
{
	if (!s_isMainThread)
	{
		Fiber *self = s_taskSchedulerData->m_perThreadData[s_threadIndex].m_currentFiber;

		{
			std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

			// early out
			if (waitGroup->m_counter == 0)
			{
				return;
			}

			// put self on waiting list...
			waitGroup->m_waitingFibers.push_back(self);
		}

		size_t fiberIdx = (size_t)self->getFiberData();

		// ...and switch to a free or resumable fiber, which will process another task
		Fiber *fiber = nullptr;
		if (!s_taskSchedulerData->m_resumableTasksQueue.try_dequeue(fiber))
		{
			while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
			{
				eastl::cpu_pause();
			}
		}

		s_taskSchedulerData->m_perThreadData[s_threadIndex].m_currentFiber = fiber;
		self->switchToFiber(*fiber);

		// we just returned from another fiber: free old fiber that we just came from
		Fiber *&oldFiber = s_taskSchedulerData->m_perFiberData[fiberIdx].m_oldFiberToPutOnFreeList;
		if (oldFiber)
		{
			s_taskSchedulerData->m_freeFibersQueue.enqueue(oldFiber);
			oldFiber = nullptr;
		}
	}
	else
	{
		uint32_t counterValue = 0;
		do
		{
			eastl::cpu_pause();
			{
				std::lock_guard<std::mutex> lock(waitGroup->m_mutex);
				counterValue = waitGroup->m_counter;
			}
			
		} while (counterValue != 0);
	}
}

size_t task::getThreadIndex()
{
	return s_threadIndex;
}

size_t task::getFiberIndex()
{
	Fiber *self = s_taskSchedulerData->m_perThreadData[s_threadIndex].m_currentFiber;
	return (size_t)self->getFiberData();
}
