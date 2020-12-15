//⬪MDPN⬪
#pragma once

#include "assert.h"
#include "dbmode.h"
#include "dbstruct.h"
#include "numset.h"

#include <core/forward.h>
#include <core/platform.h>

#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBFileIndex - индекс файлов базы данных
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBFileIndex final : AssertHelper<>
{
public:
	DBFileIndex(DataBase& data);

	// Загружает индекс из указанного файла
	bool Load(const std::wstring& filePath = L"index");
	// Сохраняет индекс в указанный файл. Если параметр saveUnexistent == true, то не будет выполнена
	// проверка существования файлов (через m_Data.ForEachChunk), а просто будут сохранены все записи
	// индекса, для которых сброшен флаг isRemoved (т.е. которые не были помечены как удалённые)
	bool Save(const std::wstring& filePath = L"index", bool saveUnexistent = false);

	// Возвращает текущий уровень указанного файла
	unsigned GetLevel(const std::wstring& filePath);
	// Устанавливает новый уровень level для указанного файла. Если параметр remove равен true, то файл
	// будет помечен как удалённый и информация о нём не будет сохранена при вызове Save. Если remove
	// равен false и произошла ошибка чтения файла или файл не существует, то функция вернёт false
	bool SetLevel(const std::wstring& filePath, unsigned level, bool remove = false);

private:
	struct FileInfo {
		static constexpr size_t PATH_LEN = DBStructure::PATH_LEN;

		uint64_t fileTime = 0;			// Время последней модификации файла
		unsigned fileSize = 0;			// Полный размер файла в байтах
		uint32_t fileCRC = 0;			// CRC32 всего файла
		char filePathA[PATH_LEN + 1];	// Путь к файлу БД (относительно директории БД)
		uint8_t level = 0;				// Уровень проверки (от 1; 0 = не проверен)
		uint8_t range = 0;				// Длина (диапазон) первого проверенного числа в файле
		bool isRemoved = false;			// true, если файл был/будет удалён (все поля кроме filePath неактуальны)
		bool isExistent = false;		// true, если файл перечисляется в m_Data.ForEachChunk (для сохранения)
	};

	using FileGroup = std::vector<FileInfo>;
	using FileIndex = FileGroup[256];

	// Находит в m_Index элемент FileInfo, соответствующий указанному файлу filePath, и возвращает
	// указатель на него. Если такого элемента нет, то возвращает nullptr. Если параметр createIfNotFound
	// равен true, то в случае отсутствия в контейнере информации о файле, создаёт для него в m_Index
	// новый пустой элемент (все поля кроме filePathA будут проинициализированы по умолчанию)
	FileInfo* GetInfo(const std::wstring& filePath, bool createIfNotFound = false);
	FileInfo* GetInfo(const char* pFilePath, bool createIfNotFound = false);
	// Обрабатывает указанную строку записи из файла индекса и добавляет для неё в m_Index новый элемент
	bool ParseRecord(const std::string& data, unsigned curRange);
	// Загружает данные в m_Index из file
	bool LoadIndex(util::File& file);
	// Сохраняет данные из m_Index в file
	bool SaveIndex(util::File& file);

	static unsigned ToNumber(char hexDigit);
	static bool AToCRC(uint32_t& out, const char* pData);
	static bool AToInt(unsigned& out, const char* pNum, size_t len);
	static bool IsGreater(const char* pLhs, size_t lhsLen, const char* pRhs, size_t rhsLen);
	static bool GetOSFileInfo(const std::wstring& filePath, uint64_t& fileSize, uint64_t& lastWriteTime);

	DataBase& m_Data;
	FileIndex m_Index;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CheckDBMode - проверка корректности файлов БД (режим работы программы)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class CheckDBMode final : public DBMode
{
public:
	CheckDBMode();

	virtual bool Run() override;

private:
	bool CheckDataBase();
	float CheckDBChunks(size_t totalChunkC);

	// Проверяет указанный файл с уровнем level. Функция должна вернуть false только в случае, если будет
	// обнаружена ошибка. Если операция была отменена, но ошибки не было, то функция должна вернуть true
	bool CheckDBChunk(DBChunk* pChunk, unsigned level);

	// Выводит сообщение об ошибке и возвращает false
	static bool OnError(const DBChunk* pChunk, unsigned level, const char* pMsg = nullptr);
	static bool OnError(const DBChunk* pChunk, unsigned level, const std::string& msg);

	bool CheckLevel2(DBChunk* pChunk);
	bool CheckLevel3(DBChunk* pChunk);
	bool CheckLevel4(DBChunk* pChunk);
	bool CheckLevel5Or6(DBChunk* pChunk, unsigned level);

	DBFileIndex m_Index;
	NumberSet m_LychThreads;

	bool m_Executed = false;			// true, если функция Run была вызвана
	bool m_IsCancelled = false;			// true, если пользователь отменил операцию
	bool m_DontRemoveBroken = false;	// если true, то некорректные файлы не удаляются
	bool m_IsIndexChanged = false;		// true, если в индекс были внесены изменения
};
