#include "Timer.h"
#include <GLFW/glfw3.h>
#include <assert.h>

uint64_t Timer::s_ticksPerSecond = 0;

void Timer::init() noexcept
{
	s_ticksPerSecond = glfwGetTimerFrequency();
}

Timer::Timer(double timeScale) noexcept
	:m_timeScale(timeScale),
	m_paused(false)
{
	assert(s_ticksPerSecond != 0);
	start();
}

void Timer::update() noexcept
{
	if (!m_paused)
	{
		m_previousElapsedTicks = m_elapsedTicks;
		m_elapsedTicks = glfwGetTimerValue() - m_startTick;
	}
}

void Timer::start() noexcept
{
	m_paused = false;
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

void Timer::pause() noexcept
{
	m_paused = true;
}

void Timer::reset() noexcept
{
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

bool Timer::isPaused() const noexcept
{
	return m_paused;
}

uint64_t Timer::getTickFrequency() const noexcept
{
	return s_ticksPerSecond;
}

uint64_t Timer::getElapsedTicks() const noexcept
{
	return m_elapsedTicks;
}

uint64_t Timer::getTickDelta() const noexcept
{
	return m_elapsedTicks - m_previousElapsedTicks;
}

double Timer::getTime() const noexcept
{
	return (m_elapsedTicks / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}

double Timer::getTimeDelta() const noexcept
{
	return ((m_elapsedTicks - m_previousElapsedTicks) / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}
