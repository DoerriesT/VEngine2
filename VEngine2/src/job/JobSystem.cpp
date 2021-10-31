#include "JobSystem.h"
#include "utility/Fiber.h"
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
	struct JobSchedulerData;
}

static JobSchedulerData *s_jobSchedulerData = nullptr;
static thread_local size_t s_threadIndex = -1;
static constexpr size_t k_numFibers = 128;
static constexpr size_t k_maxNumThreads = 64;

namespace job
{
	struct Counter
	{
		SpinLock m_mutex;
		uint32_t m_value = 0;
		eastl::bitset<k_numFibers> m_waitingFibers;
	};
}

namespace
{
	struct PerThreadData
	{
		Thread m_thread;
		moodycamel::ConcurrentQueue<Fiber *> m_resumablePinnedJobsQueue;
		Fiber m_shutdownFiber;
		Fiber *m_threadFiber;
		Fiber *m_currentFiber;
	};

	struct PerFiberData
	{
		size_t m_resumeThreadIdx = -1; // the owning fiber sets this value when it puts itself on a wait list
		Fiber *m_oldFiberToPutOnFreeList = nullptr; // other fibers set this value so that this fiber cleans them up after switch from the other to this one
		SpinLock *m_oldFiberMutexToUnlock = nullptr; // other fibers set this value so that this fiber cleans them up after switch from the other to this one
	};

	struct JobSchedulerData
	{
		eastl::array<Fiber, k_numFibers> m_fibers;
		eastl::array<PerFiberData, k_numFibers> m_perFiberData;
		eastl::array<PerThreadData, k_maxNumThreads> m_perThreadData;
		moodycamel::ConcurrentQueue<job::Job> m_jobQueue;
		moodycamel::ConcurrentQueue<Fiber *> m_resumableJobsQueue;
		moodycamel::ConcurrentQueue<Fiber *> m_freeFibersQueue;
		moodycamel::ConcurrentQueue<job::Counter *> m_freeCounters;
		eastl::atomic<uint32_t> m_stoppedThreadCount = 0;
		eastl::atomic_flag m_stopped = false;
		size_t m_threadCount = 0;
	};

	void oldFiberCleanup(size_t currentFiberIdx)
	{
		auto &fiberData = s_jobSchedulerData->m_perFiberData[currentFiberIdx];

		if (fiberData.m_oldFiberToPutOnFreeList)
		{
			s_jobSchedulerData->m_freeFibersQueue.enqueue(fiberData.m_oldFiberToPutOnFreeList);
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

		auto &threadData = s_jobSchedulerData->m_perThreadData[threadIdx];

		// convert thread to fiber to be able to run other fibers
		Fiber threadFiber = Fiber::convertThreadToFiber((void *)(50 + threadIdx));
		threadData.m_threadFiber = &threadFiber;

		// fetch a free fiber...
		Fiber *fiber = nullptr;
		while (!s_jobSchedulerData->m_freeFibersQueue.try_dequeue(fiber))
		{
		}

		// ... and switch to it: the fiber has its own loop
		s_jobSchedulerData->m_perThreadData[threadIdx].m_currentFiber = fiber;
		threadFiber.switchToFiber(*fiber);

		Log::info("Shutting down Worker Thread %u.", (unsigned int)threadIdx);
	}

	static void __stdcall mainFiberFunction(void *arg) noexcept
	{
		JobSchedulerData &schedulerData = *s_jobSchedulerData;
		size_t fiberIndex = (size_t)arg;
		Fiber *self = &schedulerData.m_fibers[fiberIndex];

		oldFiberCleanup(fiberIndex);

		while (!schedulerData.m_stopped.test())
		{
			Fiber *fiberToResume = nullptr;
			job::Job jobToExecute;
			bool foundJob = false;

			// find something to do
			while (!schedulerData.m_stopped.test())
			{
				// try to get a fiber pinned to the current thread first
				if (schedulerData.m_perThreadData[job::getThreadIndex()].m_resumablePinnedJobsQueue.try_dequeue(fiberToResume))
				{
					break;
				}

				// then try to get a fiber from the shared queue
				if (schedulerData.m_resumableJobsQueue.try_dequeue(fiberToResume))
				{
					break;
				}

				// no other jobs to resume -> fetch a fresh one
				if (schedulerData.m_jobQueue.try_dequeue(jobToExecute))
				{
					foundJob = true;
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
				schedulerData.m_perThreadData[job::getThreadIndex()].m_currentFiber = fiberToResume;
				self->switchToFiber(*fiberToResume);
				oldFiberCleanup(fiberIndex);
			}
			// found a fresh job to execute
			else if (foundJob)
			{
				// execute job
				jobToExecute.m_entryPoint(jobToExecute.m_param);

				// lock counter, decrement and enqueue resumable jobs if counter hit 0
				if (jobToExecute.m_counter)
				{
					auto &counter = *jobToExecute.m_counter;
					counter.m_mutex.lock();

					// decrement
					--counter.m_value;

					// put all waiting fibers on resumable list
					if (counter.m_value == 0)
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

							auto &resumeQueue = resumeThreadIdx == -1 ? schedulerData.m_resumableJobsQueue : schedulerData.m_perThreadData[resumeThreadIdx].m_resumablePinnedJobsQueue;
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
		self->switchToFiber(schedulerData.m_perThreadData[job::getThreadIndex()].m_shutdownFiber);

		assert(false);
	}

	static void __stdcall shutdownFiberFunction(void *) noexcept
	{
		// wait for all other threads to enter this fiber/function so that we can give each thread its original fiber back
		s_jobSchedulerData->m_stoppedThreadCount.fetch_add(1);
		while (s_jobSchedulerData->m_stoppedThreadCount.load() != s_jobSchedulerData->m_threadCount)
		{
			Thread::sleep(50);
		}

		// switch back to original fiber
		auto &threadData = s_jobSchedulerData->m_perThreadData[job::getThreadIndex()];
		threadData.m_currentFiber->switchToFiber(*threadData.m_threadFiber);

		// we should never end up here
		assert(false);
	}
}

void job::init() noexcept
{
	constexpr bool k_pinToCore = false;

	Log::info("Starting job system.");

	assert(!s_jobSchedulerData);
	s_jobSchedulerData = new JobSchedulerData();

	auto numCores = eastl::min<size_t>(Thread::getHardwareThreadCount(), s_jobSchedulerData->m_perThreadData.size());
	numCores = numCores == 0 ? 4 : numCores;
	s_jobSchedulerData->m_threadCount = numCores <= k_maxNumThreads ? numCores : k_maxNumThreads;

	// main thread fiber
	s_jobSchedulerData->m_fibers[0] = Fiber::convertThreadToFiber((void *)0 /*fiber index*/);

	// create fibers (starting from 1 because fiber 0 is our main fiber)
	for (size_t i = 1; i < k_numFibers; ++i)
	{
		s_jobSchedulerData->m_fibers[i] = Fiber(mainFiberFunction, (void *)i /*fiber index*/);
		s_jobSchedulerData->m_freeFibersQueue.enqueue(&s_jobSchedulerData->m_fibers[i]);
	}

	// set up per-thread data of main thread
	{
		s_threadIndex = 0;

		auto &threadData = s_jobSchedulerData->m_perThreadData[0];
		threadData.m_shutdownFiber = Fiber(shutdownFiberFunction, nullptr);
		threadData.m_threadFiber = &s_jobSchedulerData->m_fibers[0];
		threadData.m_currentFiber = threadData.m_threadFiber;

		// pin main thread to first core
		if (k_pinToCore)
		{
			auto res = Thread::setCoreAffinity(Thread::getCurrentThreadHandle(), 0);
			assert(res);
		}
	}

	// create worker threads (again, starting from 1)
	for (size_t i = 1; i < s_jobSchedulerData->m_threadCount; ++i)
	{
		auto &threadData = s_jobSchedulerData->m_perThreadData[i];

		// create shutdown fiber for this worker thread
		threadData.m_shutdownFiber = Fiber(shutdownFiberFunction, nullptr);

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

	Log::info("Started job system with %u threads.", (unsigned int)s_jobSchedulerData->m_threadCount);
}

void job::shutdown() noexcept
{
	assert(s_jobSchedulerData);

	Log::info("Shutting down job system.");

	s_jobSchedulerData->m_stopped.test_and_set();

	// switch to shutdown fiber, which will assign all thread fibers back to their original threads
	{
		auto &threadData = s_jobSchedulerData->m_perThreadData[job::getThreadIndex()];
		threadData.m_currentFiber->switchToFiber(threadData.m_shutdownFiber);
	}

	// we should now be on the main thread
	assert(job::getThreadIndex() == 0);

	// wait for threads to finish
	for (size_t i = 1; i < s_jobSchedulerData->m_threadCount; ++i)
	{
		bool res = s_jobSchedulerData->m_perThreadData[i].m_thread.join();
		assert(res);
	}

	// delete all counters
	Counter *counter = nullptr;
	while (s_jobSchedulerData->m_freeCounters.try_dequeue(counter))
	{
		delete counter;
	}

	delete s_jobSchedulerData;
	s_jobSchedulerData = nullptr;

	Log::info("Successfully shut down job system.");
}

void job::run(size_t count, Job *jobs, Counter **counter, Priority priority) noexcept
{
	// caller wants a counter to wait on
	if (counter)
	{
		// but does not already have one
		if (*counter == nullptr)
		{
			if (!s_jobSchedulerData->m_freeCounters.try_dequeue(*counter))
			{
				// and there were no free counters to reuse either, so allocate a new one
				*counter = new Counter();
			}
			else
			{
				assert(!(*counter)->m_mutex.status());
			}

			// fresh counter -> initialize with job count
			(*counter)->m_value = static_cast<uint32_t>(count);
		}
		// caller already has a counter -> add job count
		else
		{
			(*counter)->m_mutex.lock();
			(*counter)->m_value += static_cast<uint32_t>(count);
			(*counter)->m_mutex.unlock();
		}

		for (size_t i = 0; i < count; ++i)
		{
			jobs[i].m_counter = counter ? *counter : nullptr;
		}
	}

	s_jobSchedulerData->m_jobQueue.enqueue_bulk(jobs, count);
}

void job::waitForCounter(Counter *counter, bool stayOnThread) noexcept
{
	counter->m_mutex.lock();

	// early out
	if (counter->m_value == 0)
	{
		counter->m_mutex.unlock();
		return;
	}

	size_t threadIdx = job::getThreadIndex();
	Fiber *self = s_jobSchedulerData->m_perThreadData[threadIdx].m_currentFiber;
	const size_t fiberIdx = (size_t)self->getFiberData();

	// find a free or resumable fiber, which will process another job
	Fiber *nextFiber = nullptr;
	if (!s_jobSchedulerData->m_resumableJobsQueue.try_dequeue(nextFiber))
	{
		while (!s_jobSchedulerData->m_freeFibersQueue.try_dequeue(nextFiber))
		{
			Thread::yield();
			//eastl::cpu_pause();
		}
	}

	// put self on waiting list
	counter->m_waitingFibers.set(fiberIdx);
	s_jobSchedulerData->m_perFiberData[fiberIdx].m_resumeThreadIdx = stayOnThread ? threadIdx : -1;

	// tell next fiber to unlock our mutex after it is switched to
	s_jobSchedulerData->m_perFiberData[(size_t)nextFiber->getFiberData()].m_oldFiberMutexToUnlock = &counter->m_mutex;

	// switch fiber
	s_jobSchedulerData->m_perThreadData[threadIdx].m_currentFiber = nextFiber;
	self->switchToFiber(*nextFiber);
	oldFiberCleanup(fiberIdx);
}

void job::freeCounter(Counter *counter) noexcept
{
	counter->m_mutex.lock();
	{
		assert(counter->m_value == 0);
		assert(counter->m_waitingFibers.none());
	}
	counter->m_mutex.unlock();

	s_jobSchedulerData->m_freeCounters.enqueue(counter);
}

size_t job::getThreadIndex() noexcept
{
	return s_threadIndex;
}

size_t job::getFiberIndex() noexcept
{
	Fiber *self = s_jobSchedulerData->m_perThreadData[job::getThreadIndex()].m_currentFiber;
	return (size_t)self->getFiberData();
}

size_t job::getThreadCount() noexcept
{
	return s_jobSchedulerData->m_threadCount;
}

bool job::isManagedThread() noexcept
{
	return job::getThreadIndex() != -1;
}
