#pragma once
#include <EASTL/atomic.h>

#ifndef CONCAT_
#define CONCAT_(A, B) A ## B
#endif // CONCAT_

#ifndef CONCAT
#define CONCAT(A, B) CONCAT_(A, B)
#endif // CONCAT

#ifndef LOCK_HOLDER
#define LOCK_HOLDER(lock) SpinLockHolder CONCAT(lockHolder, __LINE__)(lock)
#endif // LOCK_HOLDER

class SpinLock
{
public:
	void lock() noexcept;
	bool try_lock() noexcept;
	void unlock() noexcept;

private:
	alignas(128) eastl::atomic_flag m_state = false;
};

class SpinLockHolder
{
public:
	explicit SpinLockHolder(SpinLock &lock);
	SpinLockHolder(const SpinLockHolder &) = delete;
	SpinLockHolder(SpinLockHolder &&) = delete;
	SpinLockHolder &operator=(const SpinLockHolder &) = delete;
	SpinLockHolder &operator=(SpinLockHolder &&) = delete;
	~SpinLockHolder();

private:
	SpinLock &m_lock;
};
