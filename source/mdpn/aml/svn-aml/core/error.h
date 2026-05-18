#pragma once

#include "../../core/platform.h"
#include "../../core/util.h"
#include "debug.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы AML_ASSERT и AML_PANIC
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Макрос AML_ASSERT предназначен только для отладки и игнорируется в релизных конфигурациях.
// Если его условие EXPRESSION равно false, то в системный журнал выводится сообщение об ошибке
// с текстом EXPRESSION и генерируется исключение EAssertion. Макрос AML_PANIC выводит сообщение
// об ошибке с текстом MESSAGE в системный журнал и генерирует исключение EHalt.

#ifdef NDEBUG
	#define AML_ASSERT(EXPRESSION) ((void) 0)
	#define AML_PANIC(MESSAGE) util::ErrorHelper::Panic(AML_WSTRING(MESSAGE))
#else
	#define AML_ASSERT(EXPRESSION) ((void)(!(EXPRESSION) && (util::ErrorHelper::Assert( \
		AML_WSTRING(__FILE__), __LINE__, AML_WSTRING(#EXPRESSION)), AML_DBGBREAK(), 0)))
	#define AML_PANIC(MESSAGE) ((void)(AML_DBGBREAK(), util::ErrorHelper::Panic( \
		AML_WSTRING(__FILE__), __LINE__, AML_WSTRING(MESSAGE)), 0))
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ErrorHelper
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace util {

//----------------------------------------------------------------------------------------------------------------------
struct ErrorHelper
{
	// Флаг режима расширенной проверки на ошибки. По умолчанию сброшен. Установка
	// флага активирует дополнительные тесты на выявление логических ошибок в коде.
	static bool isInTestMode;

public:
	static void	Assert(const wchar_t* pFile, int line, const wchar_t* pExpression);
	static void	AssertNoLog(const wchar_t* pFile, int line, const wchar_t* pExpression);

	static void	Panic(const wchar_t* pFile, int line, const wchar_t* pMessage);
	static void	Panic(const wchar_t* pMessage);

private:
	// Размер буфера в байтах (выделяется на стеке) для формирования сообщения, выводимого
	// в системный журнал, а также для генерируемого исключения функциями Assert и Panic.
	static const size_t BUFFER_SIZE = 3072;

private:
	static int	Format(wchar_t* pBuffer, size_t bufferSize, const wchar_t* pFormat, ...);
	static void	Format(wchar_t* pBuffer, size_t bufferSize, const wchar_t* pFile, int line,
					   const wchar_t* pError, const wchar_t* pMessage);

	static void	Log(char* pBuffer, const wchar_t* pFile, int line,
					const wchar_t* pError, const wchar_t* pMessage);
};

} // namespace util
