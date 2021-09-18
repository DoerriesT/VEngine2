#include "SpinLock.h"
#include <thread>
#include <random>
#include <stdint.h>

static constexpr size_t s_spinsBeforeCPUYield = 64;
static constexpr size_t s_spinsBeforeCPURelax = 0;
static constexpr size_t s_lockCongestionMaxCPURelaxCycles = 65536;

void SpinLock::lock() noexcept
{
	static thread_local std::default_random_engine e;

	size_t collisions = 0;

	while (true)
	{
		// first test, then test-and-set
		// recommended by AMD: https://gpuopen.com/gdc-presentations/2019/gdc-2019-s2-amd-ryzen-processor-software-optimization.pdf
		// also recommended by someone else: https://geidav.wordpress.com/2016/03/23/test-and-set-spinlocks/
		// atomic exchange invalidates cache lines, so doing exchange on every loop would cache thrash constantly
		// that's why we first read then try to exchange

		size_t retries = 0;
		while (m_state.test(eastl::memory_order_relaxed))
		{
			++retries;

			if (retries >= s_spinsBeforeCPUYield)
			{
				// yield is pretty aggressive, only use it after a lot of cycles
				std::this_thread::yield();
			}
			else if (retries >= s_spinsBeforeCPURelax)
			{
				// CPU relax recommended by Intel
				// 'The penalty from memory order violations can be reduced significantly by inserting a PAUSE instruction in the loop. This eliminates multiple loop iterations in the pipeline.'
				// source: https://software.intel.com/content/www/us/en/develop/articles/long-duration-spin-wait-loops-on-hyper-threading-technology-enabled-intel-processors.html
				eastl::cpu_pause();
			}
		}

		// lock is free, try to exchange
		// this can still fail if there's multiple threads at this point

		if (!m_state.test_and_set(eastl::memory_order_acquire))
		{
			// successfully acquired the lock
			return;
		}
		else
		{
			// lock was contested by another thread, try again later
			++collisions;

			// sleep longer depending on collision count, use binary exponential backoff
			uint32_t maxRelaxCycles = (uint32_t)min((size_t)1u << collisions, s_lockCongestionMaxCPURelaxCycles);
			std::uniform_int_distribution<uint32_t> d(0, maxRelaxCycles);
			uint32_t randomRelaxCycles = d(e);

			for (uint32_t i = 0; i < randomRelaxCycles; ++i)
			{
				eastl::cpu_pause();
			}
		}
	}
}

bool SpinLock::try_lock() noexcept
{
	return m_state.test_and_set(eastl::memory_order_acquire);
}

void SpinLock::unlock() noexcept
{
	m_state.clear(eastl::memory_order_release);
}

SpinLockHolder::SpinLockHolder(SpinLock &lock)
	:m_lock(lock)
{
	m_lock.lock();
}

SpinLockHolder::~SpinLockHolder()
{
	m_lock.unlock();
}
