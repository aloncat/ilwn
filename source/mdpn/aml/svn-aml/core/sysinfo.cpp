#include "sysinfo.h"

#include "../../core/winapi.h"
#include "datetime.h"
#include "debug.h"
#include "filesystem.h"

#include <string.h>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static bool GetFirstParameter(std::wstring& parameter, const wchar_t*& pCmdLine)
{
	parameter.clear();
	if (!pCmdLine) return false;

	const wchar_t* p = pCmdLine;
	while (*p == 32) ++p;

	bool allowEmpty = false;
	const wchar_t* from = p;
	for (bool quote = false; *p; ++p)
	{
		if (*p == '"')
		{
			quote = !quote;
			allowEmpty = true;
			if (p > from) parameter.append(from, p - from);
			from = p + 1;
		}
		else if ((*p == 32) && !quote)
			break;
	}

	pCmdLine = p;
	if (p > from) parameter.append(from, p - from);
	return !parameter.empty() || allowEmpty;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemInfo
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AML_IMPLEMENT_SIMPLE_SINGLETON(SystemInfo);

//----------------------------------------------------------------------------------------------------------------------
SystemInfo::SystemInfo()
{
	m_FirstTick = GetOSUptime();
	m_StartingDateTime = DateTime::Now(true);

	#if AML_OS_WINDOWS
		SYSTEM_INFO sysInfo;
		::GetSystemInfo(&sysInfo);
		m_LogicalCoreC = (sysInfo.dwNumberOfProcessors > 0) ?
			sysInfo.dwNumberOfProcessors : 1;
	#else
		#error Not implemented
	#endif

	DebugHelper::Output(DBG_INFO, "util::SystemInfo initialized");
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring SystemInfo::GetAppExePath()
{
	static size_t length = 0;
	static wchar_t exePath[320];

	if (!length)
	{
		std::wstring path;
		#if AML_OS_WINDOWS
			const wchar_t* pCmdLine = ::GetCommandLineW();
			GetFirstParameter(path, pCmdLine);
		#else
			#error Not implemented
		#endif

		if (!path.empty())
		{
			std::wstring fullPath = FileSystem::GetFullPath(path);
			if (!fullPath.empty()) path.assign(std::move(fullPath));
			if (path.size() <= sizeof(exePath) / sizeof(wchar_t))
			{
				length = path.size();
				memcpy(exePath, path.c_str(), length * sizeof(wchar_t));
			}
			return path;
		}
	}
	return std::wstring(exePath, length);
}

//----------------------------------------------------------------------------------------------------------------------
unsigned SystemInfo::GetAppUptime() const
{
	return GetOSUptime() - m_FirstTick;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE unsigned SystemInfo::GetOSUptime()
{
	#if AML_OS_WINDOWS
		uint64_t OSUptime;
		if (WinAPI::CanGetTickCount64())
			OSUptime = WinAPI::GetTickCount64();
		else
		{
			size_t last = ::GetTickCount();
			static const size_t first = last;
			OSUptime = last;
			if (last < first)
				OSUptime += 1ll << 32;
		}
		OSUptime /= 1000;
		return static_cast<unsigned>(OSUptime);
	#else
		#error Not implemented
	#endif
}

} // namespace util
