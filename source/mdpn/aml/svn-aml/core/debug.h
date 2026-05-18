#pragma once

#include "../../core/platform.h"
#include "singleton.h"
#include "threadsync.h"

#include <string>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макрос AML_DBGBREAK
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Макрос AML_DBGBREAK является аналогом интринсика __debugbreak() компилятора MSVC, но
// вызывает остановку программы только при активном отладчике. Этот макрос игнорируется
// в релизных конфигурациях.

#if AML_OS_WINDOWS && AML_DEBUG
	#define AML_DBGBREAK() ((void)(util::DebugHelper::Instance().IsDebuggerActive() && (__debugbreak(), 0)))
#else
	#define AML_DBGBREAK() ((void) 0)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DebugHelper
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace util {

enum DbgMsgType {
	DBG_NONE = 0,	// Сообщение без типа (тег "[<TYPE>]" не добавляется перед сообщением)
	DBG_INFO,		// Информационное сообщение (перед сообщением добавляется тег "[INFO]")
	DBG_WARNING,	// Предупреждение, более важное сообщение (добавляется тег "[WARNING]")
	DBG_ERROR		// Сообщение об ошибке (добавляется тег "[ERROR]")
};

enum DbgConfig {
	DBG_ENABLE_OUTPUT_IN_DEBUG = 0,	 // Работа функции Output в отладочных конфигурациях (по умолчанию true)
	DBG_ENABLE_OUTPUT_IN_RELEASE,	 // Работа функции Output в релизных конфигурациях (по умолчанию false)
	DBG_ENABLE_OUTPUT_DBG_NONE,		 // Вывод сообщений типа DBG_NONE (по умолчанию true)
	DBG_ENABLE_OUTPUT_DBG_INFO,		 // Вывод сообщений типа DBG_INFO (по умолчанию true)
	DBG_ENABLE_OUTPUT_DBG_WARNING,	 // Вывод сообщений типа DBG_WARNING (по умолчанию true)
	DBG_ENABLE_OUTPUT_DBG_ERROR,	 // Вывод сообщений типа DBG_ERROR (по умолчанию true)

	DBG_CONFIG_C
};

//----------------------------------------------------------------------------------------------------------------------
class DebugHelper
{
	AML_SIMPLE_SINGLETON(DebugHelper)

public:
	using ExitFn = void (*)(int exitCode);

public:
	// Возвращает true, если приложение работает под отладчиком. Факт наличия активного отладчика проверяется
	// однократно при инициализации класса. Для релизных конфигураций функция всегда возвращает false.
	bool			IsDebuggerActive() const { return m_IsDebuggerActive; }
	// Возвращает true для отладочных конфигураций и false для релизных.
	bool			IsDebugBuild() const { return m_IsDebugBuild; }

	void			SetAbortHandler(ExitFn handler);
	void			SetConfig(DbgConfig parameter, bool value);

	// Выводит сообщение msg (pMsg) в окно отладчика.
	static void		Output(DbgMsgType msgType, const char* pMsg);
	static void		Output(DbgMsgType msgType, const wchar_t* pMsg);
	static void		Output(DbgMsgType msgType, const std::string& msg);
	static void		Output(DbgMsgType msgType, const std::wstring& msg);

	// Аварийно завершает работу приложения, вызывая функцию _exit с указанным
	// кодом ошибки. При вызове этой функции список atexit не обрабатывается.
	[[noreturn]] static void Abort(int exitCode);

private:
	// Максимальная допустимая длина тега для типа сообщения.
	static const size_t MAX_TAG_LEN = 12;

private:
	bool			CanOutput(DbgMsgType msgType) const;

	static char*	AddTag(char* pBuffer, DbgMsgType tag);
	static void		Output(DbgMsgType msgType, char* pBuffer, size_t size);

	static void		PrintErrorMessage(const wchar_t* pErrorMessage, const wchar_t* pTitle);

private:
	thread::CriticalSection m_Lock;
	ExitFn			m_AbortHandler;
	bool			m_ConfigA[DBG_CONFIG_C];
	bool			m_IsDebuggerActive;
	bool			m_IsDebugBuild;
};

} // namespace util
