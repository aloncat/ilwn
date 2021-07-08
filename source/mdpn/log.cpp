//∙MDPN
#include "pch.h"
#include "log.h"

#include "version.h"

#include <core/array.h>
#include <core/datetime.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/winapi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogFile
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
LogFile::LogFile(const std::wstring& filePath)
{
	if (!filePath.empty())
	{
		const std::wstring dirPath = util::FileSystem::ExtractPath(filePath);
		if (!dirPath.empty() && !util::FileSystem::DirectoryExists(dirPath))
			util::FileSystem::MakeDirectory(dirPath, true);

		if (m_File.Open(filePath, util::FILE_OPEN_READWRITE | util::FILE_OPEN_ALWAYS))
		{
			const long long fileSize = m_File.GetSize();
			if (fileSize > 0)
				m_File.SetPosition(fileSize);

			util::DateTime dt;
			dt.Update(false);

			std::string s((fileSize > 0) ? "\n***\n\n" : "");
			s += util::Format("==> Log session started on %04u-%02u-%02u %02u:%02u:%02u\n",
				dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

			Print(s);
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
LogFile::~LogFile()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------
void LogFile::Print(const char* pMsg)
{
	if (pMsg && *pMsg)
	{
		thread::Lock<thread::CriticalSection> lock(m_CS);

		if (m_File.IsOpened())
		{
			Sanitize(pMsg, strlen(pMsg), [this](const char* p, size_t size) {
				m_File.Write(p, size);
			});
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void LogFile::Print(const std::string& msg)
{
	if (!msg.empty())
	{
		thread::Lock<thread::CriticalSection> lock(m_CS);

		if (m_File.IsOpened())
		{
			Sanitize(msg.c_str(), msg.size(), [this](const char* p, size_t size) {
				m_File.Write(p, size);
			});
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void LogFile::Close()
{
	thread::Lock<thread::CriticalSection> lock(m_CS);

	if (m_File.IsOpened())
	{
		Print("<== Log session closed\n");
		m_File.Close();
	}
}

//----------------------------------------------------------------------------------------------------------------------
void LogFile::Sanitize(const char* pStr, size_t strLen, const std::function<void(const char*, size_t)>& cb)
{
	util::SmartArray<char, 512> buf(strLen);

	int color = -1;
	size_t oLen = 0;
	const char* p = pStr;
	for (size_t i = 0; i < strLen; ++i)
	{
		if (p[i] == '#')
		{
			if (color == 0)
				color = -1;
			else
			{
				color = (color < 0) ? 0 : -1;
				continue;
			}
		} else
		{
			if (color >= 0)
			{
				if (p[i] >= '0' && p[i] <= '9')
				{
					if (++color == 3)
						color = -1;
					continue;
				}
				color = -1;
			}
			if (p[i] < 32 && p[i] != '\n')
				continue;
		}
		buf[oLen++] = p[i];
	}
	cb(buf, oLen);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemLog
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AML_SINGLETON(SystemLog)
wchar_t* SystemLog::s_pFilePath = nullptr;

//----------------------------------------------------------------------------------------------------------------------
SystemLog::SystemLog()
	: LogFile(GetFilePath())
{
	std::string buildVer = GetAppVersion();
	Print(util::Format("[INFO] Executable was built on %s\n", buildVer.c_str()));
}

//----------------------------------------------------------------------------------------------------------------------
void SystemLog::SetPath(const std::wstring& filePath)
{
	static auto doOnce = atexit(Tidy);

	Tidy();
	if (const size_t len = filePath.size())
	{
		s_pFilePath = new wchar_t[len + 1];
		wmemcpy(s_pFilePath, filePath.c_str(), len);
		s_pFilePath[len] = 0;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SystemLog::Tidy()
{
	AML_SAFE_DELETEA(s_pFilePath);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring SystemLog::GetFilePath()
{
	if (s_pFilePath)
		return s_pFilePath;

	constexpr size_t MAX_PATH_LEN = 1024;
	util::DynamicArray<wchar_t> buffer(MAX_PATH_LEN);
	DWORD len = ::GetModuleFileNameW(0, buffer, MAX_PATH_LEN);
	// NB: если размер буфера окажется недостаточным, то строка exePath будет
	// пустой, функция вернёт пустую строку, и файл журнала не будет создан
	std::wstring exePath(buffer, (len < MAX_PATH_LEN) ? len : 0);
	return util::FileSystem::ChangeExtension(exePath, L"log.txt");
}
