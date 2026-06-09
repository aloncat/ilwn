//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "../../../aml/core/filesystem.h"

#include <core/platform.h>

#include <string>
#include <vector>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
struct FileSystemEx : public FileSystem
{
public:
	static const wchar_t* const DELIMITER;

	struct FileTime {
		uint64_t	creationTime;	 // Дата и время создания файла
		uint64_t	lastAccessTime;	 // Дата и время последнего обращения к файлу
		uint64_t	lastWriteTime;	 // Дата и время последней модификации файла
	};

public:
	// Добавляет в вектор list имена всех файлов в указанной директории path. Строка path может быть пустой (что
	// эквивалентно "*.*"), может задавать относительный или абсолютный путь к директории, а также оканчиваться
	// любой маской с * и ?. Если recursive равен true, то поиск по этой же маске будет также осуществлен во
	// всех вложенных директориях (маска применяется только к файлам и не влияет на поиск поддиректорий).
	static void GetFileList(const std::wstring& path, std::vector<std::wstring>& list, bool recursive = false);
	// Добавляет в вектор list имена всех поддиректорий в указанной директории path. Строка
	// path может быть пустой (что эквивалентно "*.*"), может задавать относительный или
	// абсолютный путь к директории, а также оканчиваться любой маской с * и ?.
	static void GetDirectoryList(const std::wstring& path, std::vector<std::wstring>& list);

	// Заполняет поля структуры time значениями даты/времени для указанного файла path. При ошибке
	// или если файл не существует, функция вернет false, а значения полей time буду неопределены.
	static bool				GetFileTime(const std::wstring& path, FileTime& time);

	// Возвращает полный (абсолютный) путь к текущей директории.
	static std::wstring		GetCurrentDirectory();

	// Удаляет указанную директорию, если она пуста.
	static bool				RemoveDirectory(const std::wstring& path, bool recursive = false);
protected:
	struct FindData;

	static void MakeDirectoryList(const std::wstring& fullPath, const std::wstring& userPath, FindData& data);
};

} // namespace util
