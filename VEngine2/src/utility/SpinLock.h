#pragma once
#include <EASTL/atomic.h>

class SpinLock
{
public:
	void lock() noexcept;
	bool tryLock() noexcept;
	void unlock() noexcept;

private:
	alignas(64) eastl::atomic_flag m_state = false;
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