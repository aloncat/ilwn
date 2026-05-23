#pragma once

#include "../../core/platform.h"

#include <string>
#include <vector>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
struct FileSystem
{
public:
	static const wchar_t* const DELIMITER;

	struct FileTime {
		uint64_t	creationTime;	 // Дата и время создания файла
		uint64_t	lastAccessTime;	 // Дата и время последнего обращения к файлу
		uint64_t	lastWriteTime;	 // Дата и время последней модификации файла
	};

public:
	static std::wstring GetFullPath(const std::wstring& path);
	static std::wstring RemoveTrailingSlashes(const std::wstring& path);

	static std::wstring ChangeExtension(const std::wstring& path, const std::wstring& newExtension);

	static std::wstring ExtractPath(const std::wstring& path);
	static std::wstring ExtractFilename(const std::wstring& path);
	static std::wstring ExtractExtension(const std::wstring& path);

	// Добавляет в вектор list имена всех файлов в указанной директории path. Строка path может быть пустой (что
	// эквивалентно "*.*"), может задавать относительный или абсолютный путь к директории, а также оканчиваться
	// любой маской с * и ?. Если recursive равен true, то поиск по этой же маске будет также осуществлен во
	// всех вложенных директориях (маска применяется только к файлам и не влияет на поиск поддиректорий).
	static void GetFileList(const std::wstring& path, std::vector<std::wstring>& list, bool recursive = false);
	// Добавляет в вектор list имена всех поддиректорий в указанной директории path. Строка
	// path может быть пустой (что эквивалентно "*.*"), может задавать относительный или
	// абсолютный путь к директории, а также оканчиваться любой маской с * и ?.
	static void GetDirectoryList(const std::wstring& path, std::vector<std::wstring>& list);

	static bool FileExists(const std::wstring& path);
	static bool DirectoryExists(const std::wstring& path);

	// Заполняет поля структуры time значениями даты/времени для указанного файла path. При ошибке
	// или если файл не существует, функция вернет false, а значения полей time буду неопределены.
	static bool				GetFileTime(const std::wstring& path, FileTime& time);

	// Возвращает полный (абсолютный) путь к текущей директории.
	static std::wstring		GetCurrentDirectory();

	// Создает директорию path. Если path содержит составной путь (т.е. состоящий из нескольких
	// директорий, разделенных слешами), то при createAll равном false функция попытается создать
	// только последнюю директорию пути (полагая, что остальные директории этого пути уже существуют).
	// При createAll равном true функция попытается создать все несуществующие директории составного
	// пути. Функция вернет true, если директория была успешно создана или уже существовала.
	static bool				MakeDirectory(const std::wstring& path, bool createAll = false);
	// Удаляет указанную директорию, если она пуста.
	static bool				RemoveDirectory(const std::wstring& path, bool recursive = false);

	// Удаляет указанный файл path.
	static bool				RemoveFile(const std::wstring& path);

	// Переименовывает (перемещает) файл (или директорию со всеми файлами и поддиректориями).
	// Параметр path указывает на исходный файл или директорию, newName задает новое имя или
	// новый путь (при перемещении). Перемещение работает только в пределах одного тома.
	static bool				Rename(const std::wstring& path, const std::wstring& newName);

protected:
	struct FindData;

	static void MakeDirectoryList(const std::wstring& fullPath, const std::wstring& userPath, FindData& data);
	static const wchar_t* MakeLongPath(const std::wstring& path, std::wstring& tmp);
};

} // namespace util
