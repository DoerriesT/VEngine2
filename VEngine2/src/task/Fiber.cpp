#include "Fiber.h"
#include <Windows.h>

task::Fiber task::Fiber::convertThreadToFiber(void *fiberParameter) noexcept
{
	return Fiber(::ConvertThreadToFiber(fiberParameter), fiberParameter, true);
}

task::Fiber::Fiber(FiberFunction fiberFunction, void *fiberParameter) noexcept
	:m_fiberHandle(::CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, fiberFunction, fiberParameter)),
	m_fiberParameter(fiberParameter)
{
}

task::Fiber::Fiber(void *fiberHandle, void *fiberParameter, bool createdFromThread) noexcept
	:m_fiberHandle(fiberHandle),
	m_fiberParameter(fiberParameter),
	m_createdFromThread(createdFromThread)
{
}

task::Fiber::Fiber(Fiber &&fiber) noexcept
	:m_fiberHandle(fiber.m_fiberHandle),
	m_fiberParameter(fiber.m_fiberParameter)
{
	fiber.m_fiberHandle = nullptr;
	fiber.m_fiberParameter = nullptr;
}

task::Fiber &task::Fiber::operator=(Fiber &&fiber) noexcept
{
	if (fiber.m_fiberHandle != m_fiberHandle)
	{
		if (m_fiberHandle)
		{
			::DeleteFiber(m_fiberHandle);
		}
		m_fiberHandle = fiber.m_fiberHandle;
		m_fiberParameter = fiber.m_fiberParameter;
		fiber.m_fiberHandle = nullptr;
		fiber.m_fiberParameter = nullptr;
	}
	return *this;
}

task::Fiber::~Fiber()
{
	if (m_fiberHandle)
	{
		if (m_createdFromThread)
		{
			::ConvertFiberToThread();
		}
		else
		{
			::DeleteFiber(m_fiberHandle);
		}
	}
}

void task::Fiber::switchToFiber(const Fiber &fiber) const noexcept
{
	::SwitchToFiber(fiber.m_fiberHandle);
}

void *task::Fiber::getFiberData() const noexcept
{
	return m_fiberParameter;
}