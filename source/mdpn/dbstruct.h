//⬪MDPN⬪
#pragma once

#include "assert.h"
#include "dbprogress.h"

#include <core/randgen.h>
#include <core/util.h>

#include <string>
#include <unordered_set>
#include <vector>

class DataBase;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBStructure - структура папок и файлов базы данных
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBStructure final : AssertHelper<>
{
	AML_NONCOPYABLE(DBStructure)

public:
	// Полная длина валидного пути к файлу БД (относительно корня БД). Путь состоит из 2 символов
	// имени папки, прямого слеша, 10 символов имени файла, точки и 3-символьного расширения
	static constexpr size_t PATH_LEN = 17;

	DBStructure(DataBase& db);

	// Загружает структуру файлов и папок базы данных. Возвращает список всех обнаруженных
	// файлов БД (в формате "xx/filename", где xx - номер папки, а filename - имя файла).
	// Список будет содержать все обнаруженные файлы: с корректными именами (расположением)
	// и нет. Однако как "занятые" отмечены будут только корректные папки и имена файлов
	std::vector<std::wstring> Reload(DBProgress onProgress = nullptr);

	// Возвращает true, если путь path к файлу БД (имя папки + имя файла)
	// корректны, т.е. правильной длины и состоят из корректных символов
	static bool IsValidPath(const std::wstring& path);

	// Генерирует уникальное имя для нового файла; имя сразу отмечается как занятое, поэтому если
	// файл не был создан, то нужно вызвать OnFileRemoved, чтобы освободить выделенное ему имя
	std::wstring GetNewFilePath();
	// Освобождает указанное имя файла
	void OnFileRemoved(const std::wstring& path);

	// Удаляет неиспользуемые папки в базе данных. Если параметр recursive == false, то отдельные папки
	// будут удалены, только если они пусты. При значении true папки удаляются со всем их содержимым
	void WipeUnusedFolders(bool recursive = false);

private:
	struct Folder {
		unsigned number;	// Номер от 0 до 255
		size_t fileCount;	// Кол-во файлов в папке

		Folder(unsigned n, size_t c) : number(n), fileCount(c) {}
	};

	struct NameSet {
		void Clear() { m_Hashes.clear(); }
		bool Insert(const std::wstring& path);
		void Remove(const std::wstring& path);
		bool Exists(const std::wstring& path) const;

	private:
		static unsigned GetHash(const std::wstring& path);

		std::unordered_set<unsigned> m_Hashes;
	};

	unsigned GetNextFolder();
	static unsigned ToNumber(wchar_t hexDigit);
	static unsigned GetPathNumber(const std::wstring& path);
	static void FixPathSlashes(std::wstring& path);

	DataBase& m_Db;
	math::RandGen m_RGen;
	std::vector<Folder> m_Folders;
	std::vector<uint8_t> m_SpareNumbers;
	NameSet m_FileNames;
};
