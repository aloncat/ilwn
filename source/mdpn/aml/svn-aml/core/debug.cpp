#include "debug.h"

#include "../../core/array.h"
#include "../../core/winapi.h"
#include "filesystem.h"
#include "log.h"
#include "strutil.h"
#include "sysinfo.h"

#include <stdlib.h>
#include <string.h>

using namespace util;

AML_IMPLEMENT_SIMPLE_SINGLETON(DebugHelper);

//----------------------------------------------------------------------------------------------------------------------
DebugHelper::DebugHelper()
	: m_AbortHandler(0)
	, m_IsDebuggerActive(false)
	, m_IsDebugBuild(false)
{
	m_ConfigA[DBG_ENABLE_OUTPUT_IN_DEBUG] = true;
	m_ConfigA[DBG_ENABLE_OUTPUT_IN_RELEASE] = false;
	m_ConfigA[DBG_ENABLE_OUTPUT_DBG_NONE] = true;
	m_ConfigA[DBG_ENABLE_OUTPUT_DBG_INFO] = true;
	m_ConfigA[DBG_ENABLE_OUTPUT_DBG_WARNING] = true;
	m_ConfigA[DBG_ENABLE_OUTPUT_DBG_ERROR] = true;

	#ifndef NDEBUG
		#if AML_OS_WINDOWS
			m_IsDebuggerActive = (::IsDebuggerPresent() != 0);
			if (m_IsDebuggerActive) _set_error_mode(_OUT_TO_MSGBOX);
		#endif
		m_IsDebugBuild = true;
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Abort(int exitCode)
{
	AML_DBGBREAK();
	Output(DBG_INFO, "util::DebugHelper::Abort() was called.");

	Instance().m_Lock.Enter();

	static int firstExitCode;
	static int enteredC = 0;
	if (enteredC++ == 0)
	{
		firstExitCode = exitCode;
		SystemLog::FlushSafe();

		ExitFn abortHandler = Instance().m_AbortHandler;
		if (abortHandler) abortHandler(exitCode);

		std::wstring exeFileName = FileSystem::ExtractFilename(SystemInfo::GetAppExePath());
		std::wstring errorMessage = util::Format(L"Application %s has been terminated due"
			L" to fatal error.\nPlease contact the developer for help", exeFileName.c_str());
		PrintErrorMessage(errorMessage.c_str(), L"Error: abnormal termination");
	}
	_exit(firstExitCode);
}

//----------------------------------------------------------------------------------------------------------------------
char* DebugHelper::AddTag(char* pBuffer, DbgMsgType tag)
{
	if (tag == DBG_NONE)
		return pBuffer;

	const char* pTagStr = "";
	switch (tag)
	{
		case DBG_INFO:		pTagStr = "[INFO] "; break;
		case DBG_WARNING:	pTagStr = "[WARNING] "; break;
		case DBG_ERROR:		pTagStr = "[ERROR] "; break;
	}
	const size_t tagLen = strlen(pTagStr);
	if (tagLen && (tagLen <= MAX_TAG_LEN))
	{
		pBuffer -= tagLen;
		for (size_t i = 0; i < tagLen; ++i)
			pBuffer[i] = pTagStr[i];
	}
	return pBuffer;
}

//----------------------------------------------------------------------------------------------------------------------
bool DebugHelper::CanOutput(DbgMsgType msgType) const
{
	if (m_ConfigA[m_IsDebugBuild ? DBG_ENABLE_OUTPUT_IN_DEBUG : DBG_ENABLE_OUTPUT_IN_RELEASE])
	{
		if ((msgType >= DBG_NONE) && (msgType <= DBG_ERROR))
			return m_ConfigA[DBG_ENABLE_OUTPUT_DBG_NONE + msgType];
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Output(DbgMsgType msgType, const char* pMsg)
{
	if (pMsg && *pMsg && Instance().CanOutput(msgType))
	{
		char localBuffer[640];
		FlexibleArray<char> buffer(localBuffer);

		size_t len = Strlen(pMsg);
		buffer.Grow(len + MAX_TAG_LEN + 2);
		memcpy(buffer + MAX_TAG_LEN, pMsg, len);
		Output(msgType, buffer + MAX_TAG_LEN, len);
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Output(DbgMsgType msgType, const wchar_t* pMsg)
{
	if (pMsg && *pMsg && Instance().CanOutput(msgType))
	{
		size_t msgLen = Wcslen(pMsg);
		if (msgLen <= INT_MAX)
		{
			int len = ToAnsi(0, 0, pMsg, static_cast<int>(msgLen));
			if (len > 0)
			{
				char localBuffer[640];
				FlexibleArray<char> buffer(localBuffer);

				buffer.Grow(len + MAX_TAG_LEN + 2);
				len = ToAnsi(buffer + MAX_TAG_LEN, len, pMsg, static_cast<int>(msgLen));
				if (len > 0) Output(msgType, buffer + MAX_TAG_LEN, len);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Output(DbgMsgType msgType, const std::string& msg)
{
	Output(msgType, msg.c_str());
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Output(DbgMsgType msgType, const std::wstring& msg)
{
	Output(msgType, msg.c_str());
}

//----------------------------------------------------------------------------------------------------------------------
void DebugHelper::Output(DbgMsgType msgType, char* pBuffer, size_t size)
{
	pBuffer[size] = '\n';
	pBuffer[size + 1] = 0;
	const char* pText = AddTag(pBuffer, msgType);

	#if AML_OS_WINDOWS
		::OutputDebugStringA(pText);
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
void DebugHelper::PrintErrorMessage(const wchar_t* pErrorMessage, const wchar_t* pTitle)
{
	#if AML_OS_WINDOWS
		::MessageBoxW(0, pErrorMessage, pTitle, MB_ICONERROR | MB_OK);
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
void DebugHelper::SetAbortHandler(ExitFn handler)
{
	m_AbortHandler = handler;
}

//----------------------------------------------------------------------------------------------------------------------
void DebugHelper::SetConfig(DbgConfig parameter, bool value)
{
	if ((parameter >= 0) && (parameter < DBG_CONFIG_C))
		m_ConfigA[parameter] = value;
}
