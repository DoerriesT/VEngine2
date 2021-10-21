#include "Thread.h"
#include <process.h>
#include <Windows.h>
#include <assert.h>
#include "Memory.h"
#include "WideNarrowStringConversion.h"
#include "Log.h"

namespace
{
	struct ThreadStartParams
	{
		Thread::EntryFunction m_entryFunc;
		void *m_arg;
	};
}

static unsigned int __stdcall win32ThreadStartFunction(void *lpThreadParameter)
{
	ThreadStartParams *paramsPtr = reinterpret_cast<ThreadStartParams *>(lpThreadParameter);
	ThreadStartParams params = *paramsPtr;

	delete paramsPtr;
	paramsPtr = nullptr;

	params.m_entryFunc(params.m_arg);

	return EXIT_SUCCESS;
}

void *Thread::getCurrentThreadHandle() noexcept
{
	return ::GetCurrentThread();
}

uint64_t Thread::getHardwareThreadCount() noexcept
{
	SYSTEM_INFO sysInfo;
	::GetSystemInfo(&sysInfo);
	return static_cast<uint64_t>(sysInfo.dwNumberOfProcessors);
}

void Thread::yield() noexcept
{
	::SwitchToThread();
}

void Thread::sleep(uint64_t milliseconds) noexcept
{
	::Sleep(static_cast<DWORD>(milliseconds));
}

bool Thread::setCoreAffinity(void *threadHandle, size_t coreAffinity) noexcept
{
	if (!threadHandle)
	{
		return false;
	}

	DWORD_PTR mask = 1ull << coreAffinity;
	auto res = ::SetThreadAffinityMask(threadHandle, mask);
	return res != 0;
}

Thread::Thread(EntryFunction entryFunction, void *arg, size_t stackSize, const char *name) noexcept
{
	ThreadStartParams *params = new ThreadStartParams();
	params->m_entryFunc = entryFunction;
	params->m_arg = arg;

	m_threadHandle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, (unsigned int)stackSize, win32ThreadStartFunction, params, 0, nullptr));

	// check if handle is valid
	if (m_threadHandle != 0)
	{
		if (name)
		{
			const size_t nameLen = strlen(name);
			wchar_t *nameW = ALLOC_A_T(wchar_t, nameLen + 1);
			if (!widen(name, nameLen + 1, nameW))
			{
				Log::err("Thread: Failed to widen() name!");
			}
			else
			{
				if (FAILED(::SetThreadDescription(m_threadHandle, nameW)))
				{
					Log::err("Thread: Failed to set thread name!");
				}
			}
		}
	}
}

Thread::Thread(Thread &&other) noexcept
	:m_threadHandle(other.m_threadHandle)
{
	other.m_threadHandle = nullptr;
}

Thread &Thread::operator=(Thread &&other) noexcept
{
	if (this != &other)
	{
		if (m_threadHandle)
		{
			detach();
		}

		m_threadHandle = other.m_threadHandle;
		other.m_threadHandle = nullptr;
	}
	return *this;
}

Thread::~Thread()
{
	detach();
}

bool Thread::isValid() const noexcept
{
	return m_threadHandle != 0;
}

void *Thread::getHandle() noexcept
{
	return m_threadHandle;
}

bool Thread::join() noexcept
{
	if (!m_threadHandle)
	{
		return false;
	}

	DWORD result = ::WaitForSingleObject(m_threadHandle, INFINITE);
	if (result == WAIT_OBJECT_0)
	{
		bool closeRes = ::CloseHandle(m_threadHandle);
		assert(closeRes);
		m_threadHandle = nullptr;
		return true;
	}

	if (result == WAIT_ABANDONED)
	{
		return false;
	}

	return false;
}

void Thread::detach() noexcept
{
	if (m_threadHandle)
	{
		bool closeRes = ::CloseHandle(m_threadHandle);
		assert(closeRes);
		m_threadHandle = nullptr;
	}
}
