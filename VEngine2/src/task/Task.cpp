#include "Task.h"
#include "Fiber.h"
#include <EASTL/array.h>
#include <EASTL/queue.h>
#include <EASTL/vector.h>
#include <mutex>
#include <EASTL/atomic.h>
#include <concurrentqueue.h>
#include "utility/Thread.h"
#include "Log.h"
#include "profiling/Profiling.h"

namespace
{
	struct PerThreadData;
	struct PerFiberData;
	struct TaskSchedulerData;
}

static TaskSchedulerData *s_taskSchedulerData = nullptr;
static thread_local size_t s_threadIndex = -1;

//template<typename T>
//class ConcurrentQueue
//{
//public:
//	bool try_dequeue(T &item)
//	{
//		std::lock_guard<std::mutex> lg(m_mutex);
//
//		if (!m_queue.empty())
//		{
//			item = m_queue.front();
//			m_queue.pop();
//			return true;
//		}
//		return false;
//	}
//
//	bool enqueue(T &&item)
//	{
//		std::lock_guard<std::mutex> lg(m_mutex);
//		m_queue.push(item);
//
//		return true;
//	}
//
//	bool enqueue_bulk(const T *items, size_t count)
//	{
//		std::lock_guard<std::mutex> lg(m_mutex);
//		for (size_t i = 0; i < count; ++i)
//		{
//			m_queue.push(items[i]);
//		}
//		return true;
//	}
//
//private:
//	std::mutex m_mutex;
//	eastl::queue<T> m_queue;
//};

namespace task
{
	struct WaitGroup
	{
		std::mutex m_mutex;
		uint32_t m_counter = 0;
		eastl::vector<task::Fiber *> m_waitingFibers;
		eastl::vector<eastl::vector<task::Fiber *>> m_threadPinnedFibers;
		const char *m_name = nullptr;
	};
}

namespace
{
	struct PerThreadData
	{
		Thread m_thread;
		moodycamel::ConcurrentQueue<task::Fiber *> m_resumablePinnedTasksQueue;
		task::Fiber m_shutdownFiber;
		task::Fiber *m_threadFiber;
		task::Fiber *m_currentFiber;
	};

	struct PerFiberData
	{
		task::Fiber *m_oldFiberToPutOnFreeList = nullptr;
	};

	struct TaskSchedulerData
	{
		static constexpr size_t s_numFibers = 128;
		eastl::array<task::Fiber, s_numFibers> m_fibers;
		eastl::array<PerFiberData, s_numFibers> m_perFiberData;
		eastl::array<PerThreadData, 64> m_perThreadData;
		moodycamel::ConcurrentQueue<task::Task> m_taskQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_resumableTasksQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_freeFibersQueue;
		moodycamel::ConcurrentQueue<task::WaitGroup *> m_freeWaitGroups;
		eastl::atomic<uint32_t> m_stoppedThreadCount = 0;
		eastl::atomic_flag m_stopped = false;
		size_t m_threadCount = 0;
		std::mutex m_liveWaitGroupsMutex;
		eastl::vector<task::WaitGroup *> m_liveWaitGroups;
	};

	static void __stdcall shutdownFiberFunction(void *) noexcept
	{
		// wait for all other threads to enter this fiber/function so that we can give each thread its original fiber back
		s_taskSchedulerData->m_stoppedThreadCount.fetch_add(1);
		while (s_taskSchedulerData->m_stoppedThreadCount.load() != s_taskSchedulerData->m_threadCount)
		{
			Thread::sleep(50);
		}

		// switch back to original fiber
		auto &threadData = s_taskSchedulerData->m_perThreadData[task::getThreadIndex()];
		threadData.m_currentFiber->switchToFiber(*threadData.m_threadFiber);

		// we should never end up here
		assert(false);
	}
}

void task::init()
{
	constexpr bool k_pinToCore = false;

	assert(!s_taskSchedulerData);
	s_taskSchedulerData = new TaskSchedulerData();

	auto numCores = eastl::min<size_t>(Thread::getHardwareThreadCount(), s_taskSchedulerData->m_perThreadData.size());
	numCores = numCores == 0 ? 4 : numCores;
	s_taskSchedulerData->m_threadCount = numCores;

	// main thread fiber
	s_taskSchedulerData->m_fibers[0] = Fiber::convertThreadToFiber((void *)0 /*fiber index*/);

	// create fibers (starting from 1 because fiber 0 is our main fiber)
	for (size_t i = 1; i < TaskSchedulerData::s_numFibers; ++i)
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

					// try to get a fiber pinned to the current thread first
					while (schedulerData.m_perThreadData[task::getThreadIndex()].m_resumablePinnedTasksQueue.try_dequeue(resumeFiber))
					{
						// mark current fiber to be freed
						schedulerData.m_perFiberData[(size_t)resumeFiber->getFiberData()].m_oldFiberToPutOnFreeList = self;

						// switch to new fiber
						schedulerData.m_perThreadData[task::getThreadIndex()].m_currentFiber = resumeFiber;
						self->switchToFiber(*resumeFiber);
					}

					// then try to get a fiber from the shared queue
					while (schedulerData.m_resumableTasksQueue.try_dequeue(resumeFiber))
					{
						// mark current fiber to be freed
						schedulerData.m_perFiberData[(size_t)resumeFiber->getFiberData()].m_oldFiberToPutOnFreeList = self;

						// switch to new fiber
						schedulerData.m_perThreadData[task::getThreadIndex()].m_currentFiber = resumeFiber;
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
					if (curTask.m_waitGroup)
					{
						std::lock_guard<std::mutex> lock(curTask.m_waitGroup->m_mutex);

						// decrement
						--curTask.m_waitGroup->m_counter;

						// put all waiting fibers on resumable list
						if (curTask.m_waitGroup->m_counter == 0)
						{
							// non-pinned fibers
							if (!curTask.m_waitGroup->m_waitingFibers.empty())
							{
								schedulerData.m_resumableTasksQueue.enqueue_bulk(curTask.m_waitGroup->m_waitingFibers.data(), curTask.m_waitGroup->m_waitingFibers.size());
								curTask.m_waitGroup->m_waitingFibers.clear();
							}

							// thread pinned fibers
							for (size_t j = 0; j < curTask.m_waitGroup->m_threadPinnedFibers.size(); ++j)
							{
								auto &fibers = curTask.m_waitGroup->m_threadPinnedFibers[j];
								if (!fibers.empty())
								{
									schedulerData.m_perThreadData[j].m_resumablePinnedTasksQueue.enqueue_bulk(fibers.data(), fibers.size());
									fibers.clear();
								}
							}
						}
					}
				}
				else
				{
					Thread::yield();
					//eastl::cpu_pause();
				}
			}

			// switch to shutdown fiber
			self->switchToFiber(schedulerData.m_perThreadData[task::getThreadIndex()].m_shutdownFiber);

			assert(false);
		};

		s_taskSchedulerData->m_fibers[i] = Fiber(fiberFunc, (void *)i /*fiber index*/);
		s_taskSchedulerData->m_freeFibersQueue.enqueue(&s_taskSchedulerData->m_fibers[i]);
	}


	// set up per-thread data of main thread
	{
		s_threadIndex = 0;

		auto &threadData = s_taskSchedulerData->m_perThreadData[0];
		threadData.m_shutdownFiber = task::Fiber(shutdownFiberFunction, nullptr);
		threadData.m_threadFiber = &s_taskSchedulerData->m_fibers[0];
		threadData.m_currentFiber = threadData.m_threadFiber;


		// pin main thread to first core
		if (k_pinToCore)
		{
			auto res = Thread::setCoreAffinity(Thread::getCurrentThreadHandle(), 0);
			assert(res);
		}
	}

	// create worker threads (again, starting from 1)
	for (size_t i = 1; i < s_taskSchedulerData->m_threadCount; ++i)
	{
		auto threadFunc = [](void *arg)
		{
			const size_t threadIdx = (size_t)arg;
			s_threadIndex = threadIdx;

			Log::info("Starting Worker Thread %u.", (unsigned int)threadIdx);

			auto &threadData = s_taskSchedulerData->m_perThreadData[threadIdx];

			// convert thread to fiber to be able to run other fibers
			Fiber threadFiber = Fiber::convertThreadToFiber((void *)(50 + threadIdx));
			threadData.m_threadFiber = &threadFiber;

			// fetch a free fiber...
			Fiber *fiber = nullptr;
			while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
			{
			}

			// ... and switch to it: the fiber has its own loop
			s_taskSchedulerData->m_perThreadData[threadIdx].m_currentFiber = fiber;
			threadFiber.switchToFiber(*fiber);

			Log::info("Shutting down Worker Thread %u.", (unsigned int)threadIdx);
		};

		auto &threadData = s_taskSchedulerData->m_perThreadData[i];

		// create shutdown fiber for this worker thread
		threadData.m_shutdownFiber = task::Fiber(shutdownFiberFunction, nullptr);

		// create threat name
		char threadName[64];
		sprintf_s(threadName, "Worker Thread %u", (unsigned int)i);

		// create thread
		threadData.m_thread = Thread(threadFunc, (void *)i, 0, threadName);

		// pin to core
		if (k_pinToCore)
		{
			auto res = Thread::setCoreAffinity(threadData.m_thread.getHandle(), i % numCores);
			assert(res);
		}
	}
}

void task::shutdown()
{
	assert(s_taskSchedulerData);

	Log::info("Shutting down task system.");

	s_taskSchedulerData->m_stopped.test_and_set();

	// switch to shutdown fiber, which will assign all thread fibers back to their original threads
	{
		auto &threadData = s_taskSchedulerData->m_perThreadData[task::getThreadIndex()];
		threadData.m_currentFiber->switchToFiber(threadData.m_shutdownFiber);
	}

	// we should now be on the main thread
	assert(task::getThreadIndex() == 0);

	// wait for threads to finish
	for (size_t i = 1; i < s_taskSchedulerData->m_threadCount; ++i)
	{
		bool res = s_taskSchedulerData->m_perThreadData[i].m_thread.join();
		assert(res);
	}

	// delete all wait groups
	WaitGroup *waitGroup = nullptr;
	while (s_taskSchedulerData->m_freeWaitGroups.try_dequeue(waitGroup))
	{
		delete waitGroup;
	}

	delete s_taskSchedulerData;
	s_taskSchedulerData = nullptr;

	Log::info("Successfully shut down task system.");
}

task::WaitGroup *task::allocWaitGroup(const char *name)
{
	WaitGroup *waitGroup = nullptr;
	if (!s_taskSchedulerData->m_freeWaitGroups.try_dequeue(waitGroup))
	{
		waitGroup = new WaitGroup();
		waitGroup->m_threadPinnedFibers.resize(s_taskSchedulerData->m_threadCount);
	}

	waitGroup->m_name = name;

	std::lock_guard<std::mutex> lg(s_taskSchedulerData->m_liveWaitGroupsMutex);
	s_taskSchedulerData->m_liveWaitGroups.push_back(waitGroup);

	return waitGroup;
}

void task::freeWaitGroup(WaitGroup *waitGroup)
{
	{
		std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

		assert(waitGroup->m_counter == 0);
		bool isEmpty0 = waitGroup->m_waitingFibers.empty();
		assert(isEmpty0);
		for (auto &pinnedFibers : waitGroup->m_threadPinnedFibers)
		{
			bool isEmpty = pinnedFibers.empty();
			assert(isEmpty);
		}

		waitGroup->m_name = nullptr;
	}

	{
		std::lock_guard<std::mutex> lg(s_taskSchedulerData->m_liveWaitGroupsMutex);
		auto &v = s_taskSchedulerData->m_liveWaitGroups;
		v.erase(eastl::remove(v.begin(), v.end(), waitGroup), v.end());
	}

	s_taskSchedulerData->m_freeWaitGroups.enqueue(waitGroup);
}

void task::schedule(const Task &task, WaitGroup *waitGroup, Priority priority)
{
	//std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

	if (waitGroup)
	{
		waitGroup->m_counter = 1;
	}

	Task taskCopy = task;
	taskCopy.m_waitGroup = waitGroup;

	s_taskSchedulerData->m_taskQueue.enqueue(taskCopy);
}

void task::schedule(uint32_t count, Task *tasks, WaitGroup *waitGroup, Priority priority)
{
	//std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

	if (waitGroup)
	{
		waitGroup->m_counter = count;
	}

	for (size_t i = 0; i < count; ++i)
	{
		tasks[i].m_waitGroup = waitGroup;
	}

	s_taskSchedulerData->m_taskQueue.enqueue_bulk(tasks, count);
}

void task::waitFor(WaitGroup *waitGroup, bool stayOnThread)
{
	Fiber *self = s_taskSchedulerData->m_perThreadData[task::getThreadIndex()].m_currentFiber;

	{
		std::lock_guard<std::mutex> lock(waitGroup->m_mutex);

		// early out
		if (waitGroup->m_counter == 0)
		{
			return;
		}

		// put self on waiting list...
		if (stayOnThread)
		{
			waitGroup->m_threadPinnedFibers[task::getThreadIndex()].push_back(self);
		}
		else
		{
			waitGroup->m_waitingFibers.push_back(self);
		}
	}

	size_t fiberIdx = (size_t)self->getFiberData();

	// ...and switch to a free or resumable fiber, which will process another task
	Fiber *fiber = nullptr;
	if (!s_taskSchedulerData->m_resumableTasksQueue.try_dequeue(fiber))
	{
		while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
		{
			Thread::yield();
			//eastl::cpu_pause();
		}
	}

	s_taskSchedulerData->m_perThreadData[task::getThreadIndex()].m_currentFiber = fiber;
	self->switchToFiber(*fiber);

	// we just returned from another fiber: free old fiber that we just came from
	Fiber *&oldFiber = s_taskSchedulerData->m_perFiberData[fiberIdx].m_oldFiberToPutOnFreeList;
	if (oldFiber)
	{
		s_taskSchedulerData->m_freeFibersQueue.enqueue(oldFiber);
		oldFiber = nullptr;
	}
}

size_t task::getThreadIndex()
{
	return s_threadIndex;
}

size_t task::getFiberIndex()
{
	Fiber *self = s_taskSchedulerData->m_perThreadData[task::getThreadIndex()].m_currentFiber;
	return (size_t)self->getFiberData();
}

size_t task::getThreadCount()
{
	return s_taskSchedulerData->m_threadCount;
}

bool task::isManagedThread()
{
	return task::getThreadIndex() != -1;
}
