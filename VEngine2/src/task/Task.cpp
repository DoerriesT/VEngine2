#include "Task.h"
#include "Fiber.h"
#include <EASTL/array.h>
#include <EASTL/atomic.h>
#include <EASTL/bitset.h>
#include <concurrentqueue.h>
#include "utility/Thread.h"
#include "utility/SpinLock.h"
#include "Log.h"
#include "profiling/Profiling.h"

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

namespace
{
	struct TaskSchedulerData;
}

static TaskSchedulerData *s_taskSchedulerData = nullptr;
static thread_local size_t s_threadIndex = -1;
static constexpr size_t k_numFibers = 128;
static constexpr size_t k_maxNumThreads = 64;

namespace task
{
	struct Counter
	{
		SpinLock m_mutex;
		uint32_t m_count = 0;
		eastl::bitset<k_numFibers> m_waitingFibers;
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
		size_t m_resumeThreadIdx = -1; // the owning fiber sets this value when it puts itself on a wait list
		task::Fiber *m_oldFiberToPutOnFreeList = nullptr; // other fibers set this value so that this fiber cleans them up after switch from the other to this one
		SpinLock *m_oldFiberMutexToUnlock = nullptr; // other fibers set this value so that this fiber cleans them up after switch from the other to this one
	};

	struct TaskSchedulerData
	{
		eastl::array<task::Fiber, k_numFibers> m_fibers;
		eastl::array<PerFiberData, k_numFibers> m_perFiberData;
		eastl::array<PerThreadData, k_maxNumThreads> m_perThreadData;
		moodycamel::ConcurrentQueue<task::Task> m_taskQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_resumableTasksQueue;
		moodycamel::ConcurrentQueue<task::Fiber *> m_freeFibersQueue;
		moodycamel::ConcurrentQueue<task::Counter *> m_freeCounters;
		eastl::atomic<uint32_t> m_stoppedThreadCount = 0;
		eastl::atomic_flag m_stopped = false;
		size_t m_threadCount = 0;
	};

	void oldFiberCleanup(size_t currentFiberIdx)
	{
		auto &fiberData = s_taskSchedulerData->m_perFiberData[currentFiberIdx];

		if (fiberData.m_oldFiberToPutOnFreeList)
		{
			s_taskSchedulerData->m_freeFibersQueue.enqueue(fiberData.m_oldFiberToPutOnFreeList);
			fiberData.m_oldFiberToPutOnFreeList = nullptr;
		}

		if (fiberData.m_oldFiberMutexToUnlock)
		{
			assert(fiberData.m_oldFiberMutexToUnlock->status());
			fiberData.m_oldFiberMutexToUnlock->unlock();
			fiberData.m_oldFiberMutexToUnlock = nullptr;
		}
	}

	static void workerThreadMainFunction(void *arg) noexcept
	{
		const size_t threadIdx = (size_t)arg;
		s_threadIndex = threadIdx;

		Log::info("Starting Worker Thread %u.", (unsigned int)threadIdx);

		auto &threadData = s_taskSchedulerData->m_perThreadData[threadIdx];

		// convert thread to fiber to be able to run other fibers
		task::Fiber threadFiber = task::Fiber::convertThreadToFiber((void *)(50 + threadIdx));
		threadData.m_threadFiber = &threadFiber;

		// fetch a free fiber...
		task::Fiber *fiber = nullptr;
		while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
		{
		}

		// ... and switch to it: the fiber has its own loop
		s_taskSchedulerData->m_perThreadData[threadIdx].m_currentFiber = fiber;
		threadFiber.switchToFiber(*fiber);

		Log::info("Shutting down Worker Thread %u.", (unsigned int)threadIdx);
	}

	static void __stdcall mainFiberFunction(void *arg) noexcept
	{
		TaskSchedulerData &schedulerData = *s_taskSchedulerData;
		size_t fiberIndex = (size_t)arg;
		task::Fiber *self = &schedulerData.m_fibers[fiberIndex];

		oldFiberCleanup(fiberIndex);

		while (!schedulerData.m_stopped.test())
		{
			task::Fiber *fiberToResume = nullptr;
			task::Task taskToExecute;
			bool foundTask = false;

			// find something to do
			while (!schedulerData.m_stopped.test())
			{
				// try to get a fiber pinned to the current thread first
				if (schedulerData.m_perThreadData[task::getThreadIndex()].m_resumablePinnedTasksQueue.try_dequeue(fiberToResume))
				{
					break;
				}

				// then try to get a fiber from the shared queue
				if (schedulerData.m_resumableTasksQueue.try_dequeue(fiberToResume))
				{
					break;
				}

				// no other tasks to resume -> fetch a fresh one
				if (schedulerData.m_taskQueue.try_dequeue(taskToExecute))
				{
					foundTask = true;
					break;
				}

				Thread::yield();
			}

			// found a fiber to resume
			if (fiberToResume)
			{
				// mark current fiber to be freed
				schedulerData.m_perFiberData[(size_t)fiberToResume->getFiberData()].m_oldFiberToPutOnFreeList = self;

				// switch to new fiber
				schedulerData.m_perThreadData[task::getThreadIndex()].m_currentFiber = fiberToResume;
				self->switchToFiber(*fiberToResume);
				oldFiberCleanup(fiberIndex);
			}
			// found a fresh task to execute
			else if (foundTask)
			{
				// execute task
				taskToExecute.m_entryPoint(taskToExecute.m_param);

				// lock counter, decrement and enqueue resumable tasks if counter hit 0
				if (taskToExecute.m_counter)
				{
					auto &counter = *taskToExecute.m_counter;
					counter.m_mutex.lock();

					// decrement
					--counter.m_count;

					// put all waiting fibers on resumable list
					if (counter.m_count == 0)
					{
						// we need to make a copy here because fibers might be resumed and free the Counter before we are done iterating
						auto waitingFibersCopy = counter.m_waitingFibers;
						counter.m_waitingFibers = 0;

						// we also need to unlock the mutex here and do not touch the Counter past this point for the same reason as above
						counter.m_mutex.unlock();

						size_t waitingFiberIdx = waitingFibersCopy.DoFindFirst();
						while (waitingFiberIdx != waitingFibersCopy.kSize)
						{
							auto resumeThreadIdx = schedulerData.m_perFiberData[waitingFiberIdx].m_resumeThreadIdx;

							auto &resumeQueue = resumeThreadIdx == -1 ? schedulerData.m_resumableTasksQueue : schedulerData.m_perThreadData[resumeThreadIdx].m_resumablePinnedTasksQueue;
							resumeQueue.enqueue(&schedulerData.m_fibers[waitingFiberIdx]);

							waitingFiberIdx = waitingFibersCopy.DoFindNext(waitingFiberIdx);
						}
					}
					else
					{
						counter.m_mutex.unlock();
					}
				}
			}
		}

		// switch to shutdown fiber
		self->switchToFiber(schedulerData.m_perThreadData[task::getThreadIndex()].m_shutdownFiber);

		assert(false);
	}

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

void task::init() noexcept
{
	constexpr bool k_pinToCore = false;

	assert(!s_taskSchedulerData);
	s_taskSchedulerData = new TaskSchedulerData();

	auto numCores = eastl::min<size_t>(Thread::getHardwareThreadCount(), s_taskSchedulerData->m_perThreadData.size());
	numCores = numCores == 0 ? 4 : numCores;
	s_taskSchedulerData->m_threadCount = numCores <= k_maxNumThreads ? numCores : k_maxNumThreads;

	// main thread fiber
	s_taskSchedulerData->m_fibers[0] = Fiber::convertThreadToFiber((void *)0 /*fiber index*/);

	// create fibers (starting from 1 because fiber 0 is our main fiber)
	for (size_t i = 1; i < k_numFibers; ++i)
	{
		s_taskSchedulerData->m_fibers[i] = Fiber(mainFiberFunction, (void *)i /*fiber index*/);
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
		auto &threadData = s_taskSchedulerData->m_perThreadData[i];

		// create shutdown fiber for this worker thread
		threadData.m_shutdownFiber = task::Fiber(shutdownFiberFunction, nullptr);

		// create threat name
		char threadName[64];
		sprintf_s(threadName, "Worker Thread %u", (unsigned int)i);

		// create thread
		threadData.m_thread = Thread(workerThreadMainFunction, (void *)i, 0, threadName);

		// pin to core
		if (k_pinToCore)
		{
			auto res = Thread::setCoreAffinity(threadData.m_thread.getHandle(), i % numCores);
			assert(res);
		}
	}
}

void task::shutdown() noexcept
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

	// delete all counters
	Counter *counter = nullptr;
	while (s_taskSchedulerData->m_freeCounters.try_dequeue(counter))
	{
		delete counter;
	}

	delete s_taskSchedulerData;
	s_taskSchedulerData = nullptr;

	Log::info("Successfully shut down task system.");
}

void task::run(size_t count, Task *tasks, Counter **counter, Priority priority) noexcept
{
	// caller wants a counter to wait on
	if (counter)
	{
		// but does not already have one
		if (*counter == nullptr)
		{
			if (!s_taskSchedulerData->m_freeCounters.try_dequeue(*counter))
			{
				// and there were no free counters to reuse either, so allocate a new one
				*counter = new Counter();
			}
			else
			{
				assert(!(*counter)->m_mutex.status());
			}

			// fresh counter -> initialize with task count
			(*counter)->m_count = static_cast<uint32_t>(count);
		}
		// caller already has a counter -> add task count
		else
		{
			(*counter)->m_mutex.lock();
			(*counter)->m_count += static_cast<uint32_t>(count);
			(*counter)->m_mutex.unlock();
		}

		for (size_t i = 0; i < count; ++i)
		{
			tasks[i].m_counter = counter ? *counter : nullptr;
		}
	}

	s_taskSchedulerData->m_taskQueue.enqueue_bulk(tasks, count);
}

void task::waitForCounter(Counter *counter, bool stayOnThread) noexcept
{
	counter->m_mutex.lock();

	// early out
	if (counter->m_count == 0)
	{
		counter->m_mutex.unlock();
		return;
	}

	size_t threadIdx = task::getThreadIndex();
	Fiber *self = s_taskSchedulerData->m_perThreadData[threadIdx].m_currentFiber;
	const size_t fiberIdx = (size_t)self->getFiberData();

	// find a free or resumable fiber, which will process another task
	Fiber *nextFiber = nullptr;
	if (!s_taskSchedulerData->m_resumableTasksQueue.try_dequeue(nextFiber))
	{
		while (!s_taskSchedulerData->m_freeFibersQueue.try_dequeue(nextFiber))
		{
			Thread::yield();
			//eastl::cpu_pause();
		}
	}

	// put self on waiting list
	counter->m_waitingFibers.set(fiberIdx);
	s_taskSchedulerData->m_perFiberData[fiberIdx].m_resumeThreadIdx = stayOnThread ? threadIdx : -1;

	// tell next fiber to unlock our mutex after it is switched to
	s_taskSchedulerData->m_perFiberData[(size_t)nextFiber->getFiberData()].m_oldFiberMutexToUnlock = &counter->m_mutex;

	// switch fiber
	s_taskSchedulerData->m_perThreadData[threadIdx].m_currentFiber = nextFiber;
	self->switchToFiber(*nextFiber);
	oldFiberCleanup(fiberIdx);
}

void task::freeCounter(Counter *counter) noexcept
{
	counter->m_mutex.lock();
	{
		assert(counter->m_count == 0);
		assert(counter->m_waitingFibers.none());
	}
	counter->m_mutex.unlock();

	s_taskSchedulerData->m_freeCounters.enqueue(counter);
}

size_t task::getThreadIndex() noexcept
{
	return s_threadIndex;
}

size_t task::getFiberIndex() noexcept
{
	Fiber *self = s_taskSchedulerData->m_perThreadData[task::getThreadIndex()].m_currentFiber;
	return (size_t)self->getFiberData();
}

size_t task::getThreadCount() noexcept
{
	return s_taskSchedulerData->m_threadCount;
}

bool task::isManagedThread() noexcept
{
	return task::getThreadIndex() != -1;
}
