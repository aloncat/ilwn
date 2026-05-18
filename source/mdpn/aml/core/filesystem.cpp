//⬪AML⬪
#include "pch.h"
#include "filesystem.h"

#include "array.h"
#include "winapi.h"

using namespace util;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !AML_OS_WINDOWS
	// Класс FileSystem реализован только под платформу Windows
	#error Not implemented
#endif

//----------------------------------------------------------------------------------------------------------------------
static size_t CompactPath(const wchar_t* pSrc, wchar_t* pOut, size_t outPos)
{
	size_t op = outPos;
	for (size_t i = 0, j = 0; pSrc[i]; i = j)
	{
		bool slashed = false;
		while (!slashed && pSrc[++j])
			slashed = pSrc[j] == '\\' || pSrc[j] == '/';

		if (i + 2 == j && pSrc[i] == '.' && pSrc[i + 1] == '.')
		{
			if (op && pOut[op - 1] == '\\')
				--op;
			while (op && pOut[op - 1] != '\\')
				--op;
		}
		else if (i + 1 != j || pSrc[i] != '.')
		{
			if (op && pOut[op - 1] != '\\')
				pOut[op++] = '\\';
			for (size_t k = i; k < j; ++k)
				pOut[op++] = pSrc[k];
		}

		if (slashed)
		{
			if (op && pOut[op - 1] != '\\')
				pOut[op++] = '\\';
			do ++j; while (pSrc[j] == '\\' || pSrc[j] == '/');
		}
		else if (op && pOut[op - 1] == '\\')
			--op;
	}
	return op;
}

//----------------------------------------------------------------------------------------------------------------------
static bool IsUNCPath(const std::wstring& path)
{
	if (path.size() < 2)
		return false;

	auto p = path.c_str();
	// Несмотря на то, что правильный UNC путь должен начинаться с 2 обратных слешей, ОС Windows корректно
	// работает с путями вида "//?/C:\file", в которых вместо некоторых обратных слешей используются прямые
	return (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/');
}

//----------------------------------------------------------------------------------------------------------------------
static std::wstring GetFullUNCPath(const wchar_t* pPath)
{
	const DWORD LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	DWORD len = ::GetFullPathNameW(pPath, LOCAL_SIZE, localBuffer, nullptr);
	if (len > LOCAL_SIZE)
	{
		DynamicArray<wchar_t> buffer(len);
		DWORD res = ::GetFullPathNameW(pPath, len, buffer, nullptr);
		if (res && res < len)
			return std::wstring(buffer, res);
		len = 0;
	}
	return std::wstring(localBuffer, len);
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE void MakeFullPath(std::wstring& fullPath, const std::wstring& path)
{
	size_t i = 0, op = 0, drive = 0;
	const wchar_t* pSrc = path.c_str();
	for (size_t j = 0; pSrc[j] && pSrc[j] != '\\' && pSrc[j] != '/'; ++j)
	{
		if (pSrc[j] == ':')
		{
			drive = i = j + 1;
			while (pSrc[i] == ':') ++i;
			break;
		}
	}
	bool root = pSrc[i] == '\\' || pSrc[i] == '/';
	if (root) do ++i; while (pSrc[i] == '\\' || pSrc[i] == '/');

	const size_t LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	wchar_t* pOut = localBuffer;

	if (drive && (root || drive > 2))
	{
		const size_t maxLen = drive + 1 + path.size();
		if (maxLen > LOCAL_SIZE)
			pOut = new wchar_t[maxLen];
		for (size_t j = 0; j < drive; ++j)
			pOut[j] = pSrc[j];
		pOut[drive++] = '\\';
		op = drive;
	} else
	{
		const wchar_t driveBuf[3] = { pSrc[0], ':' };
		const wchar_t* pBasePath = (drive > 0) ? driveBuf : root ? L"\\" : L".";
		op = ::GetFullPathNameW(pBasePath, LOCAL_SIZE, pOut, nullptr);
		const size_t maxLen = op + 1 + path.size();
		if (maxLen > LOCAL_SIZE)
		{
			pOut = new wchar_t[maxLen];
			if (op && op < LOCAL_SIZE)
				memcpy(pOut, localBuffer, op * sizeof(wchar_t));
			else
				op = ::GetFullPathNameW(pBasePath, static_cast<DWORD>(maxLen), pOut, nullptr);
		}
		drive = (op >= 3 && pOut[1] == ':') ? 3 : (op >= 2) ? 2 : op;
	}

	op = CompactPath(&pSrc[i], &pOut[drive], op - drive);
	fullPath.append(pOut, drive + op);

	if (pOut != localBuffer)
		delete[] pOut;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileSystem
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	const wchar_t* const FileSystem::DELIMITER = L"\\";
#else
	const wchar_t* const FileSystem::DELIMITER = L"/";
#endif

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::GetFullPath(const std::wstring& path)
{
	if (path.empty() || path.size() > 32767)
		return std::wstring();

	if (IsUNCPath(path))
		return GetFullUNCPath(path.c_str());

	std::wstring fullPath;
	MakeFullPath(fullPath, path);
	return fullPath;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::RemoveTrailingSlashes(const std::wstring& path)
{
	auto p = path.c_str();
	size_t len = path.size();

	size_t left = 0;
	// Пропускаем префикс UNC пути, если он есть
	if (len >= 2 && p[0] == '\\' && p[1] == '\\')
	{
		left = 2;
		if (len >= 4 && p[2] == '?' && p[3] == '\\')
			left = 4;
	}

	while (len > left + 1 && (p[len - 1] == '\\' || p[len - 1] == '/') && p[len - 2] != ':')
		--len;
	return path.substr(0, len);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ChangeExtension(const std::wstring& path, const std::wstring& newExtension)
{
	std::wstring res;
	if (!path.empty())
	{
		size_t pos = path.find_last_of(L".\\/:");
		if (pos != std::wstring::npos)
		{
			if (path[pos] != '.')
			{
				if (pos + 1 == path.size())
					return path;
				pos = std::wstring::npos;
			}
			// NB: если path оканчивается точкой, то считаем, что расширения
			// нет, а сама точка является частью имени файла (директории)
			else if (pos + 1 == path.size())
				pos = std::wstring::npos;
		}

		res.assign(path, 0, pos);
		if (!newExtension.empty())
		{
			res.append(1, '.');
			res.append(newExtension);
		}
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ExtractPath(const std::wstring& path)
{
	size_t pos = path.find_last_of(L"\\/:");
	if (pos == std::wstring::npos)
		return std::wstring();
	return path.substr(0, pos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ExtractFilename(const std::wstring& path)
{
	size_t pos = path.find_last_of(L"\\/:");
	if (pos == std::wstring::npos)
		return path;
	return path.substr(pos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ExtractExtension(const std::wstring& path)
{
	size_t pos = path.find_last_of(L".\\/:");
	if (pos != std::wstring::npos && path[pos] == '.')
	{
		++pos;
		if (pos < path.size())
			return path.substr(pos);
	}
	return std::wstring();
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::FileExists(const std::wstring& path)
{
	int attr = GetAttributes(path);
	return attr >= 0 && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::DirectoryExists(const std::wstring& path)
{
	int attr = GetAttributes(path);
	return attr > 0 && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE int FileSystem::GetAttributes(const std::wstring& path)
{
	std::wstring tmpPath;
	auto pLongPath = MakeLongPath(path, tmpPath);

	DWORD attr = ::GetFileAttributesW(pLongPath);
	if (attr != INVALID_FILE_ATTRIBUTES)
		return static_cast<int>(attr);

	DWORD error = ::GetLastError();
	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_INVALID_NAME)
		return -1;

	WIN32_FIND_DATAW findData;
	HANDLE handle = ::FindFirstFileW(pLongPath, &findData);
	if (handle == INVALID_HANDLE_VALUE)
		return -1;

	::FindClose(handle);
	return static_cast<int>(findData.dwFileAttributes);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE const wchar_t* FileSystem::MakeLongPath(const std::wstring& path, std::wstring& tmp)
{
	const size_t len = path.size();
	if (len < MAX_PATH || len > 32767 || IsUNCPath(path))
		return path.c_str();

	tmp.assign(L"\\\\?\\");
	MakeFullPath(tmp, path);
	return tmp.c_str();
}
