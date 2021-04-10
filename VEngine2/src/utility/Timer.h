#pragma once
#include <stdint.h>

class Timer
{
public:
	static void init() noexcept;
	explicit Timer(double timeScale = 1.0) noexcept;
	void update() noexcept;
	void start() noexcept;
	void pause() noexcept;
	void reset()noexcept ;
	bool isPaused() const noexcept;
	uint64_t getTickFrequency() const noexcept;
	uint64_t getElapsedTicks() const noexcept;
	uint64_t getTickDelta() const noexcept;
	double getTime() const noexcept;
	double getTimeDelta() const noexcept;


private:
	static uint64_t s_ticksPerSecond;
	bool m_paused = false;
	double m_timeScale = 1.0;
	uint64_t m_startTick = 0;
	uint64_t m_elapsedTicks = 0;
	uint64_t m_previousElapsedTicks = 0;
};