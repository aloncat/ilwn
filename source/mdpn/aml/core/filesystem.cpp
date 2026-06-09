//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "filesystem.h"

#include <core/array.h>
#include <core/platform.h>
#include <core/util.h>
#include <core/winapi.h>

#include <algorithm>

using namespace util;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileSystemEx (Windows)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	const wchar_t* const FileSystemEx::DELIMITER = L"\\";
#else
	const wchar_t* const FileSystemEx::DELIMITER = L"/";
#endif

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
struct FileSystem::Helper final
{
	static bool IsUNCPath(std::wstring_view path);
	static std::wstring GetFullUNCPath(const wchar_t* path);
	static void MakeFullPath(std::wstring& fullPath, WZStringView path);

private:
	static size_t CompactPath(const wchar_t* src, wchar_t* out, size_t outPos);
};

//--------------------------------------------------------------------------------------------------------------------------------
struct FileSystemEx::FindData
{
	std::wstring fileMask;
	WIN32_FIND_DATAW findData;
	std::vector<std::wstring>& fileList;

public:
	FindData(std::vector<std::wstring>& list, const wchar_t* mask)
		: fileMask(mask), fileList(list)
	{
	}

	void operator =(const FindData&) = delete;

	static AML_NOINLINE uint64_t FileTimeToDateTime(const FILETIME& t) noexcept
	{
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	}
};

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystemEx::GetCurrentDirectory()
{
	constexpr DWORD LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];

	DWORD len = ::GetCurrentDirectoryW(LOCAL_SIZE, localBuffer);
	if (len > LOCAL_SIZE)
	{
		DynamicArray<wchar_t> buffer(len);
		DWORD res = ::GetCurrentDirectoryW(len, buffer);
		if (res > 0 && res < len)
			return std::wstring(buffer, res);
		len = 0;
	}

	return std::wstring(localBuffer, len);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileSystemEx::GetFileList(const std::wstring& path, std::vector<std::wstring>& list, bool recursive)
{
	// Пользовательский путь userPath: будет началом пути всех найденных файлов.
	// Полный путь fullPath: используется для получения полного пути при поиске
	std::wstring userPath, fullPath;
	// Маска поиска data.mask используется только для файлов
	FindData data(list, L"*.*");

	if (!path.empty())
	{
		size_t pos = path.find_last_of(L"\\/:");
		if (pos == std::wstring::npos)
			pos = 0;
		else
			++pos;

		if (path.find_first_of(L"*?", pos) != std::wstring::npos)
		{
			if (pos > 0)
				userPath.assign(path, 0, pos);
			data.fileMask.assign(path, pos, std::wstring::npos);
		} else
		{
			userPath = path;
			const wchar_t c = path.back();
			if (c != '\\' && c != '/' && c != ':')
				userPath.append(1, '\\');
		}
	}

	if (!userPath.empty())
	{
		fullPath = GetFullPath(userPath);
		if (!fullPath.empty() && fullPath.back() != '\\')
			fullPath.append(1, '\\');
	} else
	{
		Helper::MakeFullPath(fullPath, L".\\");
	}

	// А теперь, собственно, поиск
	if (recursive)
	{
		MakeDirectoryList(fullPath, userPath, data);
	} else
	{
		if (fullPath.size() + data.fileMask.size() + 1 > MAX_PATH && !Helper::IsUNCPath(fullPath))
			fullPath.insert(0, L"\\\\?\\");

		HANDLE handle = ::FindFirstFileW((fullPath + data.fileMask).c_str(), &data.findData);
		if (handle != INVALID_HANDLE_VALUE)
		{
			try
			{
				do {
					if (!(data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						list.push_back(userPath + data.findData.cFileName);
				} while (::FindNextFileW(handle, &data.findData));
				::FindClose(handle);
			}
			catch (...)
			{
				::FindClose(handle);
				throw;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileSystemEx::GetDirectoryList(const std::wstring& path, std::vector<std::wstring>& list)
{
	// Пользовательский путь userPath: будет началом пути всех найденных файлов.
	// Полный путь fullPath: используется для получения полного пути при поиске
	std::wstring userPath, fullPath;
	// Маска поиска data.mask используется только для файлов
	FindData data(list, L"*.*");

	if (!path.empty())
	{
		size_t pos = path.find_last_of(L"\\/:");
		if (pos == std::wstring::npos)
			pos = 0;
		else
			++pos;

		if (path.find_first_of(L"*?", pos) != std::wstring::npos)
		{
			if (pos > 0)
				userPath.assign(path, 0, pos);
			data.fileMask.assign(path, pos, std::wstring::npos);
		} else
		{
			userPath = path;
			const wchar_t c = path.back();
			if (c != '\\' && c != '/' && c != ':')
				userPath.append(1, '\\');
		}
	}

	if (!userPath.empty())
	{
		fullPath = GetFullPath(userPath);
		if (!fullPath.empty() && fullPath.back() != '\\')
			fullPath.append(1, '\\');
	} else
	{
		Helper::MakeFullPath(fullPath, L".\\");
	}

	// А теперь, собственно, поиск
	if (fullPath.size() + data.fileMask.size() + 1 > MAX_PATH && !Helper::IsUNCPath(fullPath))
		fullPath.insert(0, L"\\\\?\\");

	HANDLE handle = ::FindFirstFileW((fullPath + data.fileMask).c_str(), &data.findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		try
		{
			do {
				const wchar_t* fn = data.findData.cFileName;
				if ((data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					(fn[0] != '.' || (fn[1] && (fn[1] != '.' || fn[2]))))
				{
					list.push_back(userPath + fn);
				}
			} while (::FindNextFileW(handle, &data.findData));
			::FindClose(handle);
		}
		catch (...)
		{
			::FindClose(handle);
			throw;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystemEx::GetFileTime(const std::wstring& path, FileTime& time)
{
	WIN32_FIND_DATAW findData;
	HANDLE handle = ::FindFirstFileW(path.c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	::FindClose(handle);
	time.creationTime = FindData::FileTimeToDateTime(findData.ftCreationTime);
	time.lastAccessTime = FindData::FileTimeToDateTime(findData.ftLastAccessTime);
	time.lastWriteTime = FindData::FileTimeToDateTime(findData.ftLastWriteTime);
	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystemEx::RemoveDirectory(const std::wstring& path, bool recursive)
{
	if (path.empty())
		return false;

	std::wstring longPath;
	const wchar_t* s = nullptr;
	const size_t len = path.size();
	if (len < 248 || len > 32767 || Helper::IsUNCPath(path))
	{
		s = path.c_str();
	} else
	{
		longPath.assign(L"\\\\?\\");
		Helper::MakeFullPath(longPath, path);
		s = longPath.c_str();
	}

	// TODO: реализовать работу параметра recursive
	return ::RemoveDirectoryW(s) != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileSystemEx::MakeDirectoryList(const std::wstring& fullPath, const std::wstring& userPath, FindData& data)
{
	// Если маска поиска равна "*.*" или "*" (т.е. если мы ищем все файлы), то мы можем найти
	// файлы и обойти поддиректории за один проход. Если же маска отличается, то для поиска
	// поддиректорий нам придется использовать вторую маску и это потребует 2 проходов
	unsigned passCount = (data.fileMask == L"*.*" || data.fileMask == L"*") ? 1 : 2;

	std::wstring fileName, tmp;
	size_t searchPathLen = fullPath.size() + std::max(size_t(4), data.fileMask.size() + 1);
	const std::wstring& searchPath = (searchPathLen > MAX_PATH && !Helper::IsUNCPath(fullPath)) ?
		tmp.assign(L"\\\\?\\").append(fullPath) : fullPath;

	for (unsigned pass = 1; pass <= passCount; ++pass)
	{
		const wchar_t* mask = (pass == 1) ? data.fileMask.c_str() : L"*.*";
		HANDLE handle = ::FindFirstFileW((searchPath + mask).c_str(), &data.findData);
		if (handle != INVALID_HANDLE_VALUE)
		{
			try
			{
				do {
					const wchar_t* fn = data.findData.cFileName;
					if (!(data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						if (pass == 1)
							data.fileList.push_back(userPath + fn);
					}
					else if (pass == passCount && (fn[0] != '.' || (fn[1] && (fn[1] != '.' || fn[2]))))
					{
						fileName.assign(fn).append(1, '\\');
						MakeDirectoryList(fullPath + fileName, userPath + fileName, data);
					}
				} while (::FindNextFileW(handle, &data.findData));
				::FindClose(handle);
			}
			catch (...)
			{
				::FindClose(handle);
				throw;
			}
		}
	}
}

#else
	#error Not implemented
#endif // AML_OS_WINDOWS
