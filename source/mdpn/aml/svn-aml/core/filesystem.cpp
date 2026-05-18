#include "filesystem.h"

#include "../../core/array.h"
#include "../../core/platform.h"
#include "../../core/util.h"
#include "../../core/winapi.h"

#include <algorithm>

using namespace util;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static size_t CompactPath(const wchar_t* pSrc, wchar_t* pOut, size_t outPos)
{
	size_t op = outPos;
	for (size_t i = 0, j = 0; pSrc[i]; i = j)
	{
		bool slashed = false;
		while (!slashed && pSrc[++j])
			slashed = (pSrc[j] == '\\') || (pSrc[j] == '/');

		if ((i + 2 == j) && (pSrc[i] == '.') && (pSrc[i + 1] == '.'))
		{
			if (op && (pOut[op - 1] == '\\')) --op;
			while (op && (pOut[op - 1] != '\\')) --op;
		}
		else if ((i + 1 != j) || (pSrc[i] != '.'))
		{
			if (op && (pOut[op - 1] != '\\')) pOut[op++] = '\\';
			for (size_t k = i; k < j; ++k) pOut[op++] = pSrc[k];
		}

		if (slashed)
		{
			if (op && (pOut[op - 1] != '\\')) pOut[op++] = '\\';
			do ++j; while ((pSrc[j] == '\\') || (pSrc[j] == '/'));
		} else
			if (op && (pOut[op - 1] == '\\')) --op;
	}
	return op;
}

//----------------------------------------------------------------------------------------------------------------------
static bool IsUNCPath(const std::wstring& path)
{
	if (path.size() < 2) return false;
	const wchar_t* pStr = path.c_str();
	return ((pStr[0] == '\\') || (pStr[0] == '/')) &&
		((pStr[1] == '\\') || (pStr[1] == '/'));
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE void MakeFullPath(std::wstring& fullPath, const std::wstring& path)
{
	size_t i = 0, op = 0, drive = 0;
	const wchar_t* pSrc = path.c_str();
	for (size_t j = 0; pSrc[j] && (pSrc[j] != '\\') && (pSrc[j] != '/'); ++j)
	{
		if (pSrc[j] == ':')
		{
			drive = i = j + 1;
			while (pSrc[i] == ':') ++i;
			break;
		}
	}
	bool root = (pSrc[i] == '\\') || (pSrc[i] == '/');
	if (root) do ++i; while ((pSrc[i] == '\\') || (pSrc[i] == '/'));

	const size_t LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	wchar_t* pOut = localBuffer;

	if ((drive > 0) && (root || (drive > 2)))
	{
		const size_t maxLen = drive + 1 + path.size();
		if (maxLen > LOCAL_SIZE) pOut = new wchar_t[maxLen];
		for (size_t j = 0; j < drive; ++j) pOut[j] = pSrc[j];
		pOut[drive++] = '\\';
		op = drive;
	} else
	{
		const wchar_t driveBuf[3] = { pSrc[0], ':' };
		const wchar_t* pBasePath = (drive > 0) ? driveBuf : root ? L"\\" : L".";
		op = ::GetFullPathNameW(pBasePath, LOCAL_SIZE, pOut, 0);
		const size_t maxLen = op + 1 + path.size();
		if (maxLen > LOCAL_SIZE)
		{
			pOut = new wchar_t[maxLen];
			if (op && (op < LOCAL_SIZE))
				memcpy(pOut, localBuffer, op * sizeof(wchar_t));
			else
				op = ::GetFullPathNameW(pBasePath, static_cast<DWORD>(maxLen), pOut, 0);
		}
		drive = ((op >= 3) && (pOut[1] == ':')) ? 3 : (op >= 2) ? 2 : op;
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

//----------------------------------------------------------------------------------------------------------------------
struct FileSystem::FindData
{
	std::wstring				mask;
	WIN32_FIND_DATAW			findData;
	std::vector<std::wstring>&	fileList;

public:
	FindData(std::vector<std::wstring>& list, const wchar_t* pMask)
		: mask(pMask), fileList(list) {}
	void operator =(const FindData&);

	static AML_NOINLINE uint64_t FileTimeToDateTime(const FILETIME& t)
	{
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	}
};

//----------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::GetCurrentDirectory()
{
	const DWORD LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	DWORD len = ::GetCurrentDirectoryW(LOCAL_SIZE, localBuffer);
	if (len > LOCAL_SIZE)
	{
		DynamicArray<wchar_t> buffer(len);
		DWORD res = ::GetCurrentDirectoryW(len, buffer);
		if ((res > 0) && (res < len)) return std::wstring(buffer, res);
		len = 0;
	}
	return std::wstring(localBuffer, len);
}

//----------------------------------------------------------------------------------------------------------------------
void FileSystem::MakeDirectoryList(const std::wstring& fullPath, const std::wstring& userPath, FindData& data)
{
	// Если маска поиска равна "*.*" или "*" (т.е. если мы ищем все файлы), то мы можем найти
	// файлы и обойти поддиректории за один проход. Если же маска отличается, то для поиска
	// поддиректорий нам придется использовать вторую маску и это потребует 2 проходов.
	unsigned passCount = ((data.mask == L"*.*") || (data.mask == L"*")) ? 1 : 2;

	std::wstring fileName, tmp;
	size_t searchPathLen = fullPath.size() + std::max(size_t(4), data.mask.size() + 1);
	const std::wstring& searchPath = ((searchPathLen > MAX_PATH) && !IsUNCPath(fullPath)) ?
		tmp.assign(L"\\\\?\\").append(fullPath) : fullPath;

	for (unsigned pass = 1; pass <= passCount; ++pass)
	{
		const wchar_t* mask = (pass == 1) ? data.mask.c_str() : L"*.*";
		HANDLE handle = ::FindFirstFileW((searchPath + mask).c_str(), &data.findData);
		if (handle != INVALID_HANDLE_VALUE)
		{
			try
			{
				do {
					const wchar_t* fnA = data.findData.cFileName;
					if ((data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{
						if (pass == 1) data.fileList.push_back(userPath + fnA);
					}
					else if ((pass == passCount) && ((fnA[0] != '.') ||
						((fnA[1] != 0) && ((fnA[1] != '.') || (fnA[2] != 0)))))
					{
						fileName.assign(fnA).append(1, '\\');
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

//----------------------------------------------------------------------------------------------------------------------
void FileSystem::GetFileList(const std::wstring& path, std::vector<std::wstring>& list, bool recursive)
{
	// Пользовательский путь userPath: будет началом пути всех найденных файлов.
	// Полный путь fullPath: используется для получения полного пути при поиске.
	std::wstring userPath, fullPath;
	// Маска поиска data.mask используется только для файлов.
	FindData data(list, L"*.*");

	if (!path.empty())
	{
		size_t pos = path.find_last_of(L"\\/:");
		if (pos == std::wstring::npos) pos = 0; else ++pos;
		if (path.find_first_of(L"*?", pos) != std::wstring::npos)
		{
			if (pos > 0) userPath.assign(path, 0, pos);
			data.mask.assign(path, pos, std::wstring::npos);
		} else
		{
			userPath = path;
			const wchar_t c = path.back();
			if ((c != '\\') && (c != '/') && (c != ':'))
				userPath.append(1, '\\');
		}
	}
	if (!userPath.empty())
	{
		fullPath = GetFullPath(userPath);
		if (!fullPath.empty() && (fullPath.back() != '\\'))
			fullPath.append(1, '\\');
	} else
		MakeFullPath(fullPath, L".\\");

	// А теперь, собственно, поиск.
	if (recursive)
		MakeDirectoryList(fullPath, userPath, data);
	else
	{
		if ((fullPath.size() + data.mask.size() + 1 > MAX_PATH) && !IsUNCPath(fullPath))
			fullPath.insert(0, L"\\\\?\\");

		HANDLE handle = ::FindFirstFileW((fullPath + data.mask).c_str(), &data.findData);
		if (handle != INVALID_HANDLE_VALUE)
		{
			try
			{
				do {
					if ((data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
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

//----------------------------------------------------------------------------------------------------------------------
void FileSystem::GetDirectoryList(const std::wstring& path, std::vector<std::wstring>& list)
{
	// Пользовательский путь userPath: будет началом пути всех найденных файлов.
	// Полный путь fullPath: используется для получения полного пути при поиске.
	std::wstring userPath, fullPath;
	// Маска поиска data.mask используется только для файлов.
	FindData data(list, L"*.*");

	if (!path.empty())
	{
		size_t pos = path.find_last_of(L"\\/:");
		if (pos == std::wstring::npos) pos = 0; else ++pos;
		if (path.find_first_of(L"*?", pos) != std::wstring::npos)
		{
			if (pos > 0) userPath.assign(path, 0, pos);
			data.mask.assign(path, pos, std::wstring::npos);
		} else
		{
			userPath = path;
			const wchar_t c = path.back();
			if ((c != '\\') && (c != '/') && (c != ':'))
				userPath.append(1, '\\');
		}
	}
	if (!userPath.empty())
	{
		fullPath = GetFullPath(userPath);
		if (!fullPath.empty() && (fullPath.back() != '\\'))
			fullPath.append(1, '\\');
	} else
		MakeFullPath(fullPath, L".\\");

	// А теперь, собственно, поиск.
	if ((fullPath.size() + data.mask.size() + 1 > MAX_PATH) && !IsUNCPath(fullPath))
		fullPath.insert(0, L"\\\\?\\");

	HANDLE handle = ::FindFirstFileW((fullPath + data.mask).c_str(), &data.findData);
	if (handle != INVALID_HANDLE_VALUE)
	{
		try
		{
			do {
				const wchar_t* fnA = data.findData.cFileName;
				if ((data.findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					(fnA[0] != '.' || (fnA[1] && (fnA[1] != '.' || fnA[2]))))
				{
					list.push_back(userPath + fnA);
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

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::GetFileTime(const std::wstring& path, FileTime& time)
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

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::MakeDirectory(const std::wstring& path, bool createAll)
{
	if (path.empty())
		return false;

	std::wstring longPath;
	const wchar_t* pLongPath;
	const size_t len = path.size();
	if ((len < 248) || (len > 32767) || IsUNCPath(path))
		pLongPath = path.c_str();
	else
	{
		longPath.assign(L"\\\\?\\");
		MakeFullPath(longPath, path);
		pLongPath = longPath.c_str();
	}

	if (::CreateDirectoryW(pLongPath, 0) != 0)
		return true;

	DWORD error = ::GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
		return true;

	if (createAll && (error == ERROR_PATH_NOT_FOUND))
	{
		size_t pos = (longPath.empty() ? path.size() : longPath.size()) - 1;
		while (pos && ((pLongPath[pos] == '\\') || (pLongPath[pos] == '/'))) --pos;
		while (pos && (pLongPath[pos] != '\\') && (pLongPath[pos] != '/')) --pos;
		while (pos && ((pLongPath[pos - 1] == '\\') || (pLongPath[pos - 1] == '/'))) --pos;

		if (pos && (pLongPath[pos - 1] != '?') && (pLongPath[pos - 1] != ':'))
		{
			std::wstring prePath(pLongPath, pos);
			return MakeDirectory(prePath, true) && (::CreateDirectoryW(pLongPath, 0) != 0);
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::RemoveDirectory(const std::wstring& path, bool recursive)
{
	if (path.empty())
		return false;

	std::wstring longPath;
	const wchar_t* pLongPath;
	const size_t len = path.size();
	if ((len < 248) || (len > 32767) || IsUNCPath(path))
		pLongPath = path.c_str();
	else
	{
		longPath.assign(L"\\\\?\\");
		MakeFullPath(longPath, path);
		pLongPath = longPath.c_str();
	}

	// TODO: реализовать работу параметра recursive
	return ::RemoveDirectoryW(pLongPath) != 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::RemoveFile(const std::wstring& path)
{
	std::wstring tmpPath;
	const wchar_t* pLongPath = MakeLongPath(path, tmpPath);

	BOOL res = ::DeleteFileW(pLongPath);
	return (res != 0);
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::Rename(const std::wstring& path, const std::wstring& newName)
{
	std::wstring tmpPath1, tmpPath2;
	const wchar_t* pOldPath = MakeLongPath(path, tmpPath1);
	const wchar_t* pNewPath = MakeLongPath(newName, tmpPath2);

	BOOL res = ::MoveFileW(pOldPath, pNewPath);
	return (res != 0);
}
