//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "threadtimer.h"

#include <core/winapi.h>

//--------------------------------------------------------------------------------------------------------------------------------
ThreadTimer::ThreadTimer()
{
	DWORD threadId = ::GetCurrentThreadId();
	m_ThreadHandle = ::OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
	Reset();
}

//--------------------------------------------------------------------------------------------------------------------------------
ThreadTimer::~ThreadTimer()
{
	if (m_ThreadHandle)
	{
		::CloseHandle(m_ThreadHandle);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void ThreadTimer::Reset()
{
	if (m_ThreadHandle)
	{
		FILETIME t1, t2, tk, tu;
		if (::GetThreadTimes(m_ThreadHandle, &t1, &t2, &tk, &tu))
		{
			m_KernelTime = (static_cast<uint64_t>(tk.dwHighDateTime) << 32) | tk.dwLowDateTime;
			m_UserTime = (static_cast<uint64_t>(tu.dwHighDateTime) << 32) | tu.dwLowDateTime;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t ThreadTimer::GetElapsed(bool reset)
{
	if (m_ThreadHandle)
	{
		FILETIME t1, t2, tk, tu;
		if (::GetThreadTimes(m_ThreadHandle, &t1, &t2, &tk, &tu))
		{
			uint64_t kernelTime = (static_cast<uint64_t>(tk.dwHighDateTime) << 32) | tk.dwLowDateTime;
			uint64_t userTime = (static_cast<uint64_t>(tu.dwHighDateTime) << 32) | tu.dwLowDateTime;
			uint64_t elapsed = (kernelTime + userTime - m_KernelTime - m_UserTime) / 10;

			if (reset)
			{
				m_KernelTime = kernelTime;
				m_UserTime = userTime;
			}

			return elapsed;
		}
	}

	return 0;
}
