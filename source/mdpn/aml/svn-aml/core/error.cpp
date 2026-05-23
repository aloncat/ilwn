#include "error.h"

#include "../../core/exception.h"
#include "log.h"
#include "strutil.h"

#include <stdarg.h>
#include <stdio.h>

using namespace util;

bool ErrorHelper::isInTestMode = false;

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void ErrorHelper::Assert(const wchar_t* pFile, int line, const wchar_t* pExpression)
{
	char buffer[BUFFER_SIZE];
	Log(buffer, pFile, line, L"Assertion failed", pExpression);

	DebugHelper& dbg = DebugHelper::Instance();
	// Если мы не под отладчиком, то сгенерируем исключение EAssertion с текстом сообщения
	// об ошибке. В отладочных конфигурациях в макросе ASSERT после вызова этой функции
	// должен следовать макрос AML_DBGBREAK для остановки выполнения.
	if (!dbg.IsDebugBuild() || !dbg.IsDebuggerActive())
		throw EAssertion(*buffer ? buffer : "Unknown error");
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void ErrorHelper::AssertNoLog(const wchar_t* pFile, int line, const wchar_t* pExpression)
{
	char buffer[BUFFER_SIZE];

	// Сформируем подробное сообщение об ошибке.
	const wchar_t* pError = L"Assertion failed";
	wchar_t* pWideBuffer = reinterpret_cast<wchar_t*>(buffer);
	const size_t wideBufferSize = BUFFER_SIZE / sizeof(wchar_t);
	Format(pWideBuffer, wideBufferSize, pFile, line, pError, pExpression);
	// И выведем это сообщение в окно отладчика.
	DebugHelper::Output(DBG_ERROR, *pWideBuffer ? pWideBuffer : pError);

	DebugHelper& dbg = DebugHelper::Instance();
	// Если мы не под отладчиком, то сгенерируем исключение EAssertion с текстом выражения
	// pExpression. В отладочных конфигурациях в макросе ASSERT после вызова этой функции
	// должен следовать макрос AML_DBGBREAK для остановки выполнения.
	if (!dbg.IsDebugBuild() || !dbg.IsDebuggerActive())
	{
		int len = ToAnsi(buffer, BUFFER_SIZE - 1, pExpression);
		buffer[(len > 0) ? len : 0] = 0;

		throw EAssertion(*buffer ? buffer : "Unknown error");
	}
}

//----------------------------------------------------------------------------------------------------------------------
int ErrorHelper::Format(wchar_t* pBuffer, size_t bufferSize, const wchar_t* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	int len = vswprintf(pBuffer, bufferSize, pFormat, args);
	if (len < 0) *pBuffer = 0;
	va_end(args);
	return len;
}

//----------------------------------------------------------------------------------------------------------------------
void ErrorHelper::Format(wchar_t* pBuffer, size_t bufferSize, const wchar_t* pFile, int line,
	const wchar_t* pError, const wchar_t* pMessage)
{
	*pBuffer = 0;
	if (pFile && *pFile)
	{
		int len = Format(pBuffer, bufferSize, L"%s in file \"%s\", line %i", pError, pFile, line);
		if (len > 0)
		{
			if (pMessage && *pMessage)
				Format(&pBuffer[len], bufferSize - len, L":\n\tExpression: %s", pMessage);
			return;
		}
	}
	if (pMessage && *pMessage)
		Format(pBuffer, bufferSize, L"%s: %s", pError, pMessage);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void ErrorHelper::Log(char* pBuffer, const wchar_t* pFile, int line,
	const wchar_t* pError, const wchar_t* pMessage)
{
	// Если pError содержит начало сообщения об ошибке, сформируем
	// подробное сообщение и выведем его в системный журнал.
	if (pError && *pError)
	{
		wchar_t* pWideBuffer = reinterpret_cast<wchar_t*>(pBuffer);
		const size_t wideBufferSize = BUFFER_SIZE / sizeof(wchar_t);
		Format(pWideBuffer, wideBufferSize, pFile, line, pError, pMessage);

		SystemLog& log = SystemLog::Instance();
		const wchar_t* pLogMsg = *pWideBuffer ? pWideBuffer : pError;
		if ((log.GetMode() & LOG_TO_DBGR) == 0) DebugHelper::Output(DBG_ERROR, pLogMsg);
		log.PutRecordSafe(LOG_FATAL, pLogMsg, true);
	}

	// Преобразуем сообщение об ошибке pMessage в Ansi и запишем его в буфер.
	int len = ToAnsi(pBuffer, BUFFER_SIZE - 1, pMessage);
	pBuffer[(len > 0) ? len : 0] = 0;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void ErrorHelper::Panic(const wchar_t* pFile, int line, const wchar_t* pMessage)
{
	char buffer[BUFFER_SIZE];
	Log(buffer, pFile, line, L"Fatal error occurred", pMessage);
	throw EHalt(*buffer ? buffer : "Unknown error");
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void ErrorHelper::Panic(const wchar_t* pMessage)
{
	Panic(0, 0, pMessage);
}
