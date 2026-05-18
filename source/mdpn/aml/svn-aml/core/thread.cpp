#include "thread.h"

#include "../../core/exception.h"
#include "../../core/winapi.h"
#include "debug.h"
#include "error.h"
#include "log.h"
#include "membarrier.h"
#include "sysinfo.h"
#include "threadsync.h"

#include <new.h>
#include <stdio.h>

#if AML_OS_WINDOWS
	#include <tlhelp32.h>
#endif

using namespace thread;

#if !AML_OS_WINDOWS
	#error Not implemented
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Thread::OSThread (Windows)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct Thread::OSThread
{
	static DWORD WINAPI OSThreadProc(void* pData)
	{
		__try
		{
			int res = 0;
			if (Thread* pThread = static_cast<Thread*>(pData))
			{
				res = RunThread(pThread);
				pThread->m_ExitCode = res;
				pThread->m_HasFinished = true;
			}
			return res;
		}
		__except(LogSEH(GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {}
		// При возникновении исключения аварийно завершаем работу.
		util::DebugHelper::Abort(1);
	}

private:
	static void LogSEH(DWORD exceptionCode)
	{
		try
		{
			util::SystemLog& log = util::SystemLog::Instance();
			util::LogRecord& rec = log.StartRecord(util::LOG_FATAL);
			try
			{
				char buffer[16];
				sprintf_s(buffer, sizeof(buffer), "%08X", exceptionCode);
				rec << "Unhandled SEH exception 0x" << buffer << " caught in thread::Thread::ThreadProc()";
			}
			catch (...) {}
			rec.End(true);
		}
		catch (...) {}
	}

	static void LogException(const char* pClassName, const char* pErrorText)
	{
		try
		{
			util::SystemLog& log = util::SystemLog::Instance();
			util::LogRecord& rec = log.StartRecord(util::LOG_FATAL);
			try
			{
				if (!pClassName || !pClassName[0]) pClassName = "unknown class";
				rec << "Unhandled exception \"" << pClassName << "\" caught in thread::Thread::ThreadProc()";
				if (pErrorText && *pErrorText) rec << ":\n\t" << pErrorText;
			}
			catch (...) {}
			rec.End(true);
		}
		catch (...) {}
	}

	static int RunThread(Thread* pThread)
	{
		try
		{
			return pThread->ThreadProc(pThread->m_pUserData);
		}
		catch (util::EGeneric& e)
		{
			LogException(e.ClassName(), e.what());
		}
		catch (std::exception& e)
		{
#ifdef _CPPRTTI
			LogException(typeid(e).name(), e.what());
#else
			LogException(0, e.what());
#endif
		}
		catch (...)
		{
			util::SystemLog::Instance().PutRecordSafe(util::LOG_FATAL, "Unrecognized"
				" unhandled exception caught in thread::Thread::ThreadProc()", true);
		}
		// При возникновении исключения аварийно завершаем работу.
		util::DebugHelper::Abort(1);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Thread::MainThread
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class Thread::MainThread : public Thread
{
public:
	static Thread& Instance()
	{
		if (Thread* pObj = m_pInstance)
		{
			ReadMemBarrier();
			return *pObj;
		}
		return *CreateObj();
	}

	static bool IsMe(const Thread* pThread)
	{
		return (pThread == m_pInstance);
	}

protected:
	MainThread() : Thread(*this) {}
	virtual int ThreadProc(void*) { return 0; }

	static AML_NOINLINE Thread* CreateObj()
	{
		static volatile char spinLock = 0;
		AutoFastSpinLock lock(spinLock);
		if (Thread* pObj = m_pInstance)
		{
			ReadMemBarrier();
			return pObj;
		}
		static uint8_t data[sizeof(MainThread)];
		Thread* pObj = new(data) MainThread;
		WriteMemBarrier();
		m_pInstance = pObj;
		return pObj;
	}

private:
	static Thread* volatile m_pInstance;
};

Thread* volatile Thread::MainThread::m_pInstance = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Thread
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
Thread::Thread(void* pUserData, bool createSuspended)
	: m_Handle(0)
	, m_pUserData(pUserData)
	, m_ThreadId(0)
	, m_ExitCode(0)
	, m_IsRunning(false)
	, m_HasFinished(false)
	, m_WasExitCalled(false)
	, m_WasShutDown(false)
{
	// Проинициализируем класс SystemInfo, если мы не сделали
	// этого раньше. И проверим, что наша система совместима.
	util::CheckMinimalRequirements();
	util::SystemInfo::Instance();

	// Проверим отсутствие рекурсии между классом SystemLog и классом потока
	// Thread. Это необходимо сделать, так как в случае ошибки создания потока
	// мы будем использовать синглтон системного журнала (AML_PANIC).
	CheckRecursion();

	// Инициализируем объект главного потока.
	MainThread::Instance();

	DWORD threadId = 0;
	// Создаем поток в приостановленном состоянии, чтобы гарантировано получить его handle и
	// ID до того, как поток сможет вызвать любую из функций класса, использующих их значения.
	m_Handle = ::CreateThread(0, 0, OSThread::OSThreadProc, this, CREATE_SUSPENDED, &threadId);
	if (!m_Handle) AML_PANIC("Failed to create new thread");
	m_ThreadId = threadId;

	// Возобновляем работу созданного потока.
	if (!createSuspended)
	{
		DWORD res = ::ResumeThread(m_Handle);
		// Если нам не удалось запустить поток, сгенерируем исключение EHalt. Но перед этим
		// терминируем поток и закроем его хэндл, так как деструктор класса вызван не будет.
		if (res == DWORD(-1))
		{
			::TerminateThread(m_Handle, 0);
			::CloseHandle(m_Handle);
			m_ThreadId = 0;
			m_Handle = 0;

			AML_PANIC("Failed to start new thread");
		} else
			// Все в пордяке, поток запущен.
			m_IsRunning = true;
	}
}

//----------------------------------------------------------------------------------------------------------------------
Thread::Thread(MainThread&)
	: m_Handle(0)
	, m_pUserData(0)
	, m_ThreadId(0)
	, m_ExitCode(0)
	, m_IsRunning(true)
	, m_HasFinished(false)
	, m_WasExitCalled(false)
	, m_WasShutDown(false)
{
	// Делаем снимок всех потоков и находим в нем самый первый поток нашего процесса.
	// Этот первый поток и должен быть главным потоком (если он еще не был завершен).
	HANDLE handle = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (handle != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 entry;
		entry.dwSize = sizeof(entry);
		const DWORD processId = ::GetCurrentProcessId();
		for (BOOL success = ::Thread32First(handle, &entry); success; success = ::Thread32Next(handle, &entry))
		{
			if (entry.th32OwnerProcessID == processId)
			{
				m_ThreadId = entry.th32ThreadID;
				break;
			}
		}
		::CloseHandle(handle);
	}

	// Если по какой-то причине нам не удалось получить ID первого
	// потока процесса, то главным будем считать текущий поток.
	if (m_ThreadId == 0) m_ThreadId = ::GetCurrentThreadId();

	// Получаем хэндл главного потока.
	m_Handle = ::OpenThread(SYNCHRONIZE | THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION |
		THREAD_SUSPEND_RESUME | THREAD_TERMINATE, FALSE, m_ThreadId);

	// Если мы находимся не в контексте главного потока, то главный поток может быть в
	// приостановленном состоянии. Проверим это и скорректируем значение m_IsRunning.
	if (m_Handle && (m_ThreadId != ::GetCurrentThreadId()))
	{
		// Пробуем приостановить поток.
		if (::SuspendThread(m_Handle) != DWORD(-1))
		{
			// И снова возобновляем его.
			DWORD count = ::ResumeThread(m_Handle);
			// Если count будет больше 1, значит поток был
			// неактивен на момент вызова SuspendThread.
			m_IsRunning = (count <= 1);

			// Если нам не удалось возобновить поток, сгенерируем исключение EHalt. Но
			// перед этим закроем хэндл потока, так как деструктор класса вызван не будет.
			if (count == DWORD(-1))
			{
				::CloseHandle(m_Handle);
				m_ThreadId = 0;
				m_Handle = 0;

				AML_PANIC("Failed to resume main thread");
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
Thread::~Thread()
{
	if (m_Handle)
	{
		ShutDownAndWait();
		::CloseHandle(m_Handle);
		m_ThreadId = 0;
		m_Handle = 0;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Thread::CheckRecursion()
{
	static bool isDone = false;
	static bool recursionDetected = false;
	if (util::ErrorHelper::isInTestMode && !isDone)
	{
		static volatile long testThreadId = 0;
		long threadId = ::GetCurrentThreadId();
		::InterlockedCompareExchange(&testThreadId, threadId, 0);

		// Проверку будет выполнять только один поток, который
		// первым проинициализировал значение testThreadId.
		if (threadId && (threadId == testThreadId))
		{
			if (recursionDetected)
			{
				util::DebugHelper::Output(util::DBG_ERROR, "Detected SystemLog <--> Thread recursion");
				util::DebugHelper::Abort(1);
			}

			recursionDetected = true;
			util::SystemLog& log = util::SystemLog::Instance();
			log.PutRecord(util::LOG_INFO, "SystemLog <--> Thread recursion check done");

			isDone = true;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Thread::Exit()
{
	m_WasExitCalled = true;
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t Thread::GetCurrentThreadId()
{
	return ::GetCurrentThreadId();
}

//----------------------------------------------------------------------------------------------------------------------
Thread& Thread::GetMainThread()
{
	return MainThread::Instance();
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t Thread::GetThreadId() const
{
	return m_HasFinished ? 0 : m_ThreadId;
}

//----------------------------------------------------------------------------------------------------------------------
bool Thread::Resume()
{
	if (!m_Handle || m_HasFinished)
		return false;

	if (m_IsRunning)
		return true;

	DWORD count = ::ResumeThread(m_Handle);
	if (count == DWORD(-1))
	{
		if (m_HasFinished) return false;
		AML_PANIC("Failed to resume thread");
	}
	m_IsRunning = (count <= 1);
	return m_IsRunning;
}

//----------------------------------------------------------------------------------------------------------------------
void Thread::SetPriority(int priority)
{
	static const int win32PriorityA[7] = {
		THREAD_PRIORITY_IDLE,
		THREAD_PRIORITY_LOWEST,
		THREAD_PRIORITY_BELOW_NORMAL,
		THREAD_PRIORITY_NORMAL,
		THREAD_PRIORITY_ABOVE_NORMAL,
		THREAD_PRIORITY_HIGHEST,
		THREAD_PRIORITY_TIME_CRITICAL
	};

	if (m_Handle && !m_HasFinished)
	{
		int idx = 3 + util::Clamp(priority, -3, 3);
		::SetThreadPriority(m_Handle, win32PriorityA[idx]);
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Thread::ShutDownAndWait()
{
	if (!m_WasShutDown && !MainThread::IsMe(this) &&
		m_Handle && (m_ThreadId != ::GetCurrentThreadId()))
	{
		m_WasShutDown = true;

		DWORD exitCode;
		// Если поток еще не завершился, попытаемся его завершить.
		if ((::GetExitCodeThread(m_Handle, &exitCode) == 0) || (exitCode == STILL_ACTIVE))
		{
			Exit();
			// Если поток спит, терминируем его без пробуждения. Иначе ждем пока он не завершится сам.
			if (!m_IsRunning || (::WaitForSingleObject(m_Handle, INFINITE) != WAIT_OBJECT_0))
				::TerminateThread(m_Handle, 0);
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool Thread::Suspend()
{
	if (!m_Handle || m_HasFinished)
		return false;

	if (m_ThreadId == ::GetCurrentThreadId())
	{
		m_IsRunning = false;
		DWORD count = ::SuspendThread(m_Handle);
		if (count == DWORD(-1))
		{
			m_IsRunning = true;
			AML_PANIC("Failed to suspend thread");
		}
		return true;
	}

	DWORD count = ::SuspendThread(m_Handle);
	if (count == DWORD(-1))
	{
		if (m_HasFinished) return false;
		AML_PANIC("Failed to suspend thread");
	}
	m_IsRunning = false;
	return !m_HasFinished;
}

//----------------------------------------------------------------------------------------------------------------------
void Thread::Terminate(int exitCode)
{
	if (m_Handle && !m_HasFinished)
	{
		uint32_t threadId = ::GetCurrentThreadId();

		m_ExitCode = exitCode;
		if (m_ThreadId == threadId)
			m_HasFinished = true;
		if (::TerminateThread(m_Handle, exitCode) != 0)
			m_HasFinished = true;
		if (m_ThreadId == threadId)
			m_HasFinished = false;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool Thread::WaitFor(unsigned milliseconds) const
{
	if (m_Handle && !m_HasFinished)
	{
		DWORD timeToWait = milliseconds;
		if (milliseconds == 0) timeToWait = INFINITE;
		if (::WaitForSingleObject(m_Handle, timeToWait) != WAIT_OBJECT_0)
			return false;
	}
	return true;
}
