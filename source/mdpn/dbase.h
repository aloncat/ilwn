//⬪MDPN⬪
#pragma once

#include "assert.h"
#include "const.h"
#include "dbchunk.h"
#include "dbchunklist.h"
#include "dbprogress.h"
#include "dbstruct.h"
#include "number.h"

#include <core/platform.h>
#include <core/util.h>

#include <functional>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DataBase - база данных для хранения результатов поиска
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DataBase final
{
	AML_NONCOPYABLE(DataBase)

public:
	DataBase();
	~DataBase();

	// Инициализирует базу данных. Если БД пуста (или её директория не найдена), то при createNewDb, равном
	// false, функция также вернёт false; а если БД была успешно загружена, то вернёт true. Если параметр
	// createNewDb равен true, и БД не была найдена, то будет создана пустая БД и функция вернёт true; если
	// же БД существовала, то функция, напротив, вернёт false. Параметр dataState задаёт уровень, до которого
	// данные файлов будут выгружены из памяти при завершении инициализации БД. Если параметр path задан, то
	// путь path будет использован в качестве пути к директории базы данных и/или её имени; если path не задан
	// или содержит только имя без пути (оканчивающаяся слешем строка рассматривается как путь без имени), то
	// БД ищется во всех директориях, начиная с текущей и до корневой; имя директории по умолчанию - "data"
	bool Init(bool createNewDb = false, DBChunkState dataState = DBChunkState::DATAUNLOADED,
		const std::wstring& path = L"");
	// Безопасная инициализация базы данных. Должна использоваться только в режиме проверки БД. В отличие
	// от обычной инициализации, SafeInit не сгенерирует исключение и не вернёт false в случае, если один
	// или несколько файлов не будут загружены. При этом статистическая информация будет некорректна
	bool SafeInit(const std::wstring& path = L"");

	// Возвращает полный путь к директории БД. Всегда оканчивается слешем
	const std::wstring& GetBasePath() const;

	unsigned GetHighestStep() const { return m_HighestStep; }
	bool HasFound(unsigned step) const { return step <= Const::MAX_STEP && m_FoundStepA[step]; }
	// Возвращает количество первичных чисел Лишрел для указанного диапазона, отмеченных в БД
	uint64_t GetLychrelC(size_t range) const { return (range <= Const::MAX_DIGIT_C) ? m_PrimLychA[range] : 0; }

	// Возвращает true, если в диапазоне числа GetLast() до него есть непроверенные интервалы чисел.
	// Значение обновляется при инициализации базы данных (Init), а также при вызове функции Save
	bool HasGaps() const { return m_HasGaps; }
	// Возвращает последнее проверенное число в базе данных. Значение обновляется
	// при инициализации базы данных (Init), а также при вызове функции Save
	const Number& GetLast() const { return m_Last; }

	// Делает pChunk текущим активным файлом
	void SetActiveChunk(DBChunk* pChunk);

	// Уведомляет о том, что найден палиндром с низким шагом step, который не будет сохранён
	// в базу данных. Эта функция может использоваться только при формировании нового файла
	void AddPalindrome(unsigned step);
	// Добавляет новый отложенный палиндром num для шага step. Эта функция может использоваться
	// для "дополнения" файла палиндромами с низкими шагами, которые не сохранялись ранее
	void AddPalindrome(const Number& num, unsigned step);
	// Обновляет статистику по числам Лишрел. Параметр num - число Лишрел, stepC - количество
	// операций RAA, выполненное для его проверки (оно же - минимальная глубина поиска)
	void AddLychrel(const Number& num, unsigned stepC);

	// Сохраняет текущий активный файл, если он соедржит несохранённые изменения. Параметр last должен
	// содержать последнее проверенное число интервала. Параметр minSavedStep должен содержать значение
	// шага, начиная с которого палиндромы сохранялись в файл. Параметр timeSpent должен быть равен
	// суммарному времени CPU (в ms), которое было затрачено на проверку добавленных в файл данных
	void Save(const Number& last, unsigned minSavedStep, unsigned timeSpent);

	// Возвращает количество файлов в базе данных
	size_t GetChunkC() const { return m_Chunks.GetSize(); }
	// Вызывает пользовательскую функцию fn для каждого файла БД. Гарантируется, что файлы будут
	// перечисляться по возрастанию их значений GetFirst(). Функция fn должна вернуть false, если
	// обход файлов нужно прекратить, или вернуть true, если нужно продолжить
	void ForEachChunk(const std::function<bool(DBChunk*)>& fn);

	// Уничтожает объект pChunk и удаляет соответствующий ему файл
	bool RemoveChunk(DBChunk* pChunk);

private:
	// По аналогии с обработкой ошибок в классе DBChunk, EE::Assert и EE::Verify
	// в DataBase используются для контроля корректности использования класса БД
	using EE = AssertHelper<EDBLogic>;

	// Ищет базу данных по указанному пути и инициализирует m_BasePath правильным
	// значением. Если существующая БД будет найдена, то функция вернёт true
	bool FindBasePath(const std::wstring& path);
	// Перемещает (и переименовывает) все невалидные файлы в dbFiles так, чтобы они стали валидными
	void RearrangeInvalidFiles(std::vector<std::wstring>& dbFiles, DBProgress onProgress = nullptr);
	// Загружает заголовки файлов БД, инициализирует список файлов m_Chunk и значение m_Last
	void LoadFileHeaders(std::vector<std::wstring>& dbFiles, DBProgress onProgress = nullptr);
	// Загружает статистику файлов БД, инициализирует остальные поля класса. Параметр dataState
	// задаёт уровень, до которого данные будут выгружены из памяти после завершения загрузки
	void LoadStatistics(DBChunkState dataState, DBProgress onProgress = nullptr);

	std::wstring m_BasePath;
	DBStructure m_Structure;
	DBChunkList m_Chunks;

	bool m_IsInitialized = false;
	bool m_IsInitializing = false;
	bool m_SafeInitMode = false;

	bool m_HasGaps = false;				// true, если в диапазоне числа m_Last есть непроверенные числа до m_Last
	unsigned m_HighestStep = 0;			// Наибольший шаг среди всех найденных отложенных палиндромов в БД
	bool* m_FoundStepA = nullptr;		// Флаги наличия в БД хотя бы 1 числа для каждого шага
	uint64_t* m_PrimLychA = nullptr;	// Кол-во первичных чисел Лишрел для каждого диапазона
	DBChunk* m_pActiveChunk = nullptr;	// Текущий активный (дополняемый данными) файл
	Number m_Last;						// Последнее проверенное число в базе данных
};
