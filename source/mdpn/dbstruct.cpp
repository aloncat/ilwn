//∙MDPN
#include "pch.h"
#include "dbstruct.h"

#include "dbase.h"

#include <core/exception.h>
#include <core/fasthash.h>
#include <core/filesystem.h>
#include <core/platform.h>
#include <core/strutil.h>

//----------------------------------------------------------------------------------------------------------------------
DBStructure::DBStructure(DataBase& db)
	: m_Db(db)
{
	m_SpareNumbers.resize(256);
	for (unsigned i = 0; i < 256; ++i)
		m_SpareNumbers[i] = i & 0xff;
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<std::wstring> DBStructure::Reload(DBProgress onProgress)
{
	m_Folders.clear();
	m_SpareNumbers.clear();
	m_FileNames.Clear();

	onProgress(0);
	std::vector<std::wstring> folders, files, dbFiles, dups;
	folders.push_back(util::FileSystem::RemoveTrailingSlashes(m_Db.GetBasePath()));
	util::FileSystem::GetDirectoryList(m_Db.GetBasePath() + L"*.*", folders);

	size_t counterA[256];
	AML_FILLA(counterA, 0, 256);

	files.reserve(500);
	const size_t folderC = folders.size();
	const size_t predictedFileC = folderC * (3 * folderC / 2);
	dbFiles.reserve(util::Clamp(predictedFileC, size_t(50), size_t(50000)));

	for (size_t i = 0; i < folderC; ++i)
	{
		onProgress(1.f + 97.f * i / folderC);
		// NB: самая первая папка списка - корневая папка БД, только её
		// сканируем нерекурсивно, остальные папки сканируем рекурсивно
		util::FileSystem::GetFileList(folders[i] + L"/*.*", files, i > 0);

		for (auto& filePath : files)
		{
			const auto ext = util::FileSystem::ExtractExtension(filePath);
			// TODO: ".db" - это старое расширение файлов формата БД версии 3. Как только вся
			// база будет сконвертирована в новый формат, можно будет убрать это расширение
			if (!util::StrInsCmp(ext, L"pal") || !util::StrInsCmp(ext, L"db"))
			{
				filePath.erase(0, m_Db.GetBasePath().size());
				FixPathSlashes(filePath);

				dbFiles.push_back(filePath);
				if (IsValidPath(filePath))
				{
					if (m_FileNames.Insert(filePath))
						++counterA[GetPathNumber(filePath)];
					else
					{
						dbFiles.pop_back();
						dups.push_back(std::move(filePath));
					}
				}
			}
		}
		files.clear();
	}

	onProgress(98.f);
	m_Folders.reserve(32);
	for (unsigned number = 0; number < 256; ++number)
	{
		if (counterA[number])
			m_Folders.emplace_back(number, counterA[number]);
		else
			m_SpareNumbers.push_back(number & 0xff);
	}
	// NB: папки должны быть отсортированы по возрастанию количества файлов в них
	std::sort(m_Folders.begin(), m_Folders.end(), [](const Folder& lhs, const Folder& rhs) {
		return lhs.fileCount < rhs.fileCount;
	});

	onProgress(99.f);
	for (const auto& dup : dups)
	{
		const auto newPath = GetNewFilePath();
		if (!util::FileSystem::Rename(m_Db.GetBasePath() + dup, m_Db.GetBasePath() + newPath))
			throw util::ERuntime("Failed to rename/move database file");
		dbFiles.push_back(newPath);
	}

	return dbFiles;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBStructure::IsValidPath(const std::wstring& path)
{
	// Корректный путь файла БД состоит и 2-символьного имени папки, слеша и 10-символьного имени файла
	// с расширением "pal", где каждый символ имени папки и файла - любая из цифр 16-тиричного алфавита
	if (path.size() != PATH_LEN || (path[2] != '\\' && path[2] != '/') || path[13] != '.')
		return false;

	if (util::StrInsCmp(util::FileSystem::ExtractExtension(path), L"pal"))
		return false;

	for (size_t i = 0; i < 13; ++i)
	{
		const unsigned c = path[i];
		if (i != 2 && (c < '0' || c > '9') && (c < 'a' || c > 'f') && (c < 'A' || c > 'F'))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring DBStructure::GetNewFilePath()
{
	std::wstring path;
	path.reserve(PATH_LEN);

	const wchar_t* digitA = L"0123456789abcdef";
	const unsigned folder = GetNextFolder();
	path += digitA[(folder >> 4) & 0xf];
	path += digitA[folder & 0xf];

	auto fullPath = m_Db.GetBasePath() + path;
	// NB: проверка существования папки работает в разы быстрее, чем попытка создания уже существующей папки.
	// Так как обычно папки создаются всего 1 раз, то такая проверка значительно ускоряет работу функции
	if (!util::FileSystem::DirectoryExists(fullPath))
		util::FileSystem::MakeDirectory(fullPath, true);

	for (path += L"/1234567890.pal";;)
	{
		do {
			for (size_t i = 0; i < 10; ++i)
				path[3 + i] = digitA[m_RGen.UInt() & 0xf];
		} while (!m_FileNames.Insert(path));
		if (!util::FileSystem::FileExists(m_Db.GetBasePath() + path))
			break;
		m_FileNames.Remove(path);
	}

	Assert(path.size() == PATH_LEN);
	return path;
}

//----------------------------------------------------------------------------------------------------------------------
void DBStructure::OnFileRemoved(const std::wstring& path)
{
	if (!IsValidPath(path))
		return;

	m_FileNames.Remove(path);

	const unsigned number = GetPathNumber(path);
	for (size_t i = 0; i < m_Folders.size(); ++i)
	{
		Folder& folder = m_Folders[i];
		if (folder.number != number)
			continue;

		if (folder.fileCount <= 1)
		{
			util::FileSystem::RemoveDirectory(m_Db.GetBasePath() + path.substr(0, 2));
			m_SpareNumbers.push_back(number & 0xff);
			m_Folders.erase(m_Folders.begin() + i);
		} else
		{
			const size_t count = --folder.fileCount;
			// При необходимости переместим изменённый элемент влево,
			// чтобы восстановить неубывающий порядок элементов
			size_t left = i;
			while (left && m_Folders[left - 1].fileCount > count)
				--left;
			std::swap(m_Folders[left], folder);
		}

		break;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DBStructure::WipeUnusedFolders(bool recursive)
{
	if (!m_SpareNumbers.empty())
	{
		const wchar_t* digitA = L"0123456789abcdef";
		std::wstring path = m_Db.GetBasePath() + L"12";

		for (unsigned n : m_SpareNumbers)
		{
			path[path.size() - 2] = digitA[(n >> 4) & 0xf];
			path[path.size() - 1] = digitA[n & 0xf];

			util::FileSystem::RemoveDirectory(path, recursive);
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBStructure::GetNextFolder()
{
	Assert(!m_Folders.empty() || !m_SpareNumbers.empty());

	const size_t halfSize = m_Folders.size() / 2;
	// NB: вектор m_Folders отсортирован по возрастанию количества файлов в папках
	if (halfSize < 4 || (m_Folders[halfSize].fileCount >= 3 * m_Folders.size() / 2 && !m_SpareNumbers.empty()))
	{
		const size_t spareC = m_SpareNumbers.size();
		const size_t i = m_RGen.UInt(static_cast<unsigned>(spareC));

		const unsigned number = m_SpareNumbers[i];
		m_SpareNumbers.erase(m_SpareNumbers.begin() + i);
		m_Folders.emplace(m_Folders.begin(), number, 1);
		return number;
	}

	const size_t index = m_RGen.UInt(static_cast<unsigned>(halfSize));
	const size_t count = ++m_Folders[index].fileCount;

	// При необходимости переместим выбранный элемент вправо,
	// чтобы восстановить неубывающий порядок элементов
	auto right = m_Folders.begin() + index;
	for (auto it = right + 1; it != m_Folders.end() && count > it->fileCount; ++it)
		right = it;
	std::swap(m_Folders[index], *right);

	return right->number;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBStructure::ToNumber(wchar_t hexDigit)
{
	const unsigned c = hexDigit;
	if (c >= '0' && c <= '9')
		return c - 48;
	if (c >= 'a' && c <= 'f')
		return c - 87;
	if (c >= 'A' && c <= 'F')
		return c - 55;
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBStructure::GetPathNumber(const std::wstring& path)
{
	return (path.size() >= 2) ? 16 * ToNumber(path[0]) + ToNumber(path[1]) : 0;
}

//----------------------------------------------------------------------------------------------------------------------
void DBStructure::FixPathSlashes(std::wstring& path)
{
	for (auto& c : path)
	{
		if (c == '\\')
			c = '/';
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool DBStructure::NameSet::Insert(const std::wstring& path)
{
	unsigned hash = GetHash(path);
	return m_Hashes.insert(hash).second;
}

//----------------------------------------------------------------------------------------------------------------------
void DBStructure::NameSet::Remove(const std::wstring& path)
{
	unsigned hash = GetHash(path);
	m_Hashes.erase(hash);
}

//----------------------------------------------------------------------------------------------------------------------
bool DBStructure::NameSet::Exists(const std::wstring& path) const
{
	unsigned hash = GetHash(path);
	return m_Hashes.find(hash) != m_Hashes.end();
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE unsigned DBStructure::NameSet::GetHash(const std::wstring& path)
{
	if (path.size() < 13)
		return 0;

	wchar_t buffer[11];
	const wchar_t* p = path.c_str();
	for (size_t i = 0; i < 10; ++i)
		buffer[i] = p[3 + i];
	buffer[10] = 0;

	return hash::GetFastHash(buffer, true);
}
