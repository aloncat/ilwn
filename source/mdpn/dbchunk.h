//⬪MDPN⬪
#pragma once

#include "assert.h"
#include "number.h"

#include <core/forward.h>
#include <core/platform.h>
#include <core/util.h>

#include <string>
#include <vector>

class DBChunkAccessor;
class DataBase;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Перечисление состояний данных/изменений (DBChunkState) и вспомогательный класс DBChunkFlags
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
enum class DBChunkState : uint8_t {
	UNINITIALIZED = 0,	// Класс DBChunk не проинициализирован (никакие поля не заданы)
	FILEPATHONLY,		// Задан только путь и имя файла (член m_FilePath), остальное не определено
	DATAUNLOADED,		// Данные выгружены, память освобождена (члены m_FilePath и m_First заданы)
	HEADERONLY,			// Заголовок файла полностью загружен, статистика и блок данных не загружены
	WITHSTATS,			// Загружен заголовок файла и блок статистики, блок данных не загружен
	FULLDATA,			// Загружены все данные из файла

	UNCHANGED = 0,		// Изменений нет (сохранение в файл не требуется)
	HEADERCHANGED,		// Изменился только заголовок (статистику и блок данных сохранять не требуется)
	DATACHANGED			// Данные изменились (требуется перезаписать в файл все данные)
};

//----------------------------------------------------------------------------------------------------------------------
class DBChunkFlags final
{
public:
	enum Flag {
		HAS_FILE_PATH		= 1 << 0,	// Путь и имя файла заданы
		OLD_FORMAT_VER		= 1 << 1,	// Файл имеет устаревший формат и должен быть обновлен
		IS_NEW_CHUNK		= 1 << 2,	// DBChunk не синхронизирован с файлом, а инициализирован для новых данных
		DATA_SORTED			= 1 << 3,	// Числа в DataItems гарантированно отсортированы по возрастанию
		HAS_OWNER			= 1 << 4	// Объектом DBChunk владеет m_ChunkList класса DataBase
	};

	DBChunkFlags(unsigned flags) : m_Flags(static_cast<Data>(flags)) {}

	void Set(unsigned flags) { m_Flags |= static_cast<Data>(flags); }
	bool Check(unsigned flags) const { return (m_Flags & flags) == flags; }
	void Clear(unsigned flags) { m_Flags = static_cast<Data>(m_Flags & ~flags); }

private:
	using Data = uint16_t;
	Data m_Flags;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunkData - данные файла БД (заголовок, блок статистики, блок данных)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBChunkData final : AssertHelper<>
{
	AML_NONCOPYABLE(DBChunkData)

public:
	// Для удобства заголовок файла БД имеет фиксированный размер и сохраняется в текстовом
	// формате. Оставшееся неиспользованным простраство заголовка заполняется символами #
	static constexpr size_t FILE_HEADER_SIZE = 400;

	// Элемент данных
	struct DataItem {
		FixNumber num;	// Число - отложенный палиндром
		unsigned step;	// Количество шагов, за которое число становится палиндромом

		bool operator <(const DataItem& rhs) const { return num < rhs.num; }
	};
	// Контейнер блока данных
	using DataItems = std::vector<DataItem>;

	DBChunkData(DBChunkAccessor& owner);
	~DBChunkData();

	// Инициализирует пустой объект для накопления данных и последующего сохранения в новый файл.
	// Параметр reserveCount позволяет сразу зарезервировать память под указанное количество
	// элементов, чтобы избежать потом ненужных ресайзов вектора при добавлении палиндромов
	void Init(size_t reserveCount = 0);

	using State = DBChunkState;
	using Flag = DBChunkFlags::Flag;
	// Загружает указанное количество данных из файла. Если данные уже загружены, то ничего не делает.
	// Если при загрузке произойдёт ошибка, то функция вернёт false, а состояние объекта не изменится
	bool LoadData(util::File& file, State stateNeeded);
	// Выгружает данные из памяти, оставляя только указанное (не ниже
	// HEADERONLY) количество. Объект не должен содержать изменений
	void UnloadData(State stateNeeded);

	// Сохраняет данные в файл: заголовок сохраняется всегда, блоки статистики и данных
	// сохраняются при наличии изменений в данных или если forceFullSave равен true
	bool Save(util::File& file, bool forceFullSave = false);

	unsigned GetFormatVer() const { return m_FormatVer; }
	const Number& GetLast() const { return m_Last; }
	void SetLast(const Number& num);

	uint64_t GetIterationC() const { return m_IterationC; }
	uint64_t GetPrimaryLychrelC() const { return m_PrimaryLychrelC; }
	const Number& GetAllLychrelC();
	uint64_t GetSavedPalindromeC() const { return m_SavedPalC; }
	const Number& GetAllSavedPalindromeC();
	unsigned GetCPUTimeSpent() const { return m_CPUTimeSpent; }
	unsigned GetMinSavedStep() const { return m_MinSavedStep; }
	unsigned GetSearchDepth() const { return m_SearchDepth; }

	// Возвращает размер несжатых/сжатых данных/полный размер файла. Значения
	// актуальны, только если в блоке данных нет несохранённых изменений
	unsigned GetDataSize() const { return m_DataSize; }
	unsigned GetCDataSize() const { return m_CDataSize; }
	unsigned GetFileSize() const;

	void SetCPUTimeSpent(unsigned cpuTime);
	void SetMinSavedStep(unsigned minSavedStep);

	const unsigned* GetNumCountA() const { return m_NumCountA; }
	unsigned GetHighestStep() const { return m_HighestStep; }
	// Вычисляет полное количество сохранённых чисел
	// в файле (на основе блока статистики)
	size_t GetTotalNumberC() const;

	const DataItems& GetNumbers() const { return *m_pData; }
	// Сортирует числа в DataItems по возрастанию
	void SortNumbers();

	// Уведомляет о том, что найден палиндром с низким шагом, который не будет сохранён в
	// базу данных. Эта функция может использоваться только при формировании нового файла
	void AddPalindrome() { ++m_IterationC; }
	// Добавляет новый отложенный палиндром num для шага step. Эта функция может использоваться
	// для "дополнения" файла палиндромами с низкими шагами, которые не сохранялись ранее
	void AddPalindrome(const Number& num, unsigned step);
	// Обновляет статистику по числам Лишрел. Параметр num - число Лишрел, stepC - количество
	// операций RAA, выполненное для его проверки (оно же - минимальная глубина поиска)
	void AddLychrel(const Number& num, unsigned stepC);

	// Удаляет из блока данных все сохранённые палиндромы с шагом ниже minStep.
	// Возвращает true, если был удалён хотя бы один отложенный палиндром
	bool RemovePalindromes(unsigned minStep);

private:
	static constexpr unsigned LATEST_FORMAT_VERSION = 5;
	// В то время как обычные Assert/Verify используются для контроля логических ошибок в
	// самом классе, EE::Assert и EE::Verify генерируют при ошибках исключение EDBLogic и
	// предназначены для контроля корректности использования классов DBChunk/DBChunkData
	using EE = AssertHelper<EDBLogic>;

	bool ReloadHeader(util::File& file);
	bool LoadStatBlock(util::File& file);
	bool LoadDataBlock(util::File& file);
	bool ParseStats(const char* pData);
	bool ParseData(const char* pData);

	bool SaveHeader(util::File& file);
	void SaveStats(std::string& out);
	bool SaveData(util::File& out);

	DBChunkAccessor& m_Chunk;

	uint32_t m_HeaderCRC = 0;		// CRC32 заголовка загруженного файла
	unsigned m_FormatVer = 0;		// Версия формата загруженного файла

	// Заголовок
	Number m_Last;					// Последнее число проверенного интервала
	uint64_t m_IterationC = 0;		// Количество проверенных чисел (итераций) в интервале
	uint64_t m_PrimaryLychrelC = 0;	// Количество первичных чисел Лишрел, найденных в интервале
	uint64_t m_SavedPalC = 0;		// Количество отложенных палиндромов, сохранённых в блоке данных
	uint64_t m_AllLychrelIntC = 0;	// Полное количество чисел Лишрел, найденных в интервале (инкр. часть)
	Number m_AllLychrelNumC;		// Полное количество чисел Лишрел, найденных в интервале (основное значение)
	uint64_t m_AllSavedPalIntC = 0;	// Полное кол-во отложенных палиндромов, сохранённых в блоке данных (инкр. часть)
	Number m_AllSavedPalNumC;		// Полное кол-во отложенных палиндромов, сохранённых в блоке данных (осн. значение)
	unsigned m_CPUTimeSpent = 0;	// Количество времени CPU в ms, потраченного на поиск чисел в интервале
	unsigned m_MinSavedStep = 0;	// Минимальный шаг, начиная с которого сохраняются палиндромы
	unsigned m_SearchDepth = 0;		// Минимальная глубина поиска (проверки кандидатов в числа Лишрел)

	unsigned m_StatSize = 0;		// Размер блока статистики в байтах (расположен сразу за заголовком)
	uint32_t m_StatCRC = 0;			// CRC32 блока статистики (statSize байт, начиная с позиции FILE_HEADER_SIZE)

	unsigned m_DataSize = 0;		// Размер несжатого блока данных в байтах
	uint32_t m_DataCRC = 0;			// CRC несжатого блока данных (dataSize байт)
	unsigned m_CDataSize = 0;		// Размер сжатого блока данных (расположен сразу за блоком статистики)

	// Блок статистики
	unsigned* m_NumCountA = nullptr;	// Количество найденных в интервале палиндромов для каждого шага
	unsigned m_HighestStep = 0;			// Наибольший найденный в интервале шаг

	// Блок данных
	DataItems* m_pData = nullptr;		// Массив всех найденных в интервале палиндромов
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunk - основной класс, обособленная часть БД, хранимая в отдельном файле
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBChunk final : AssertHelper<>
{
	friend class DBChunkAccessor;
	AML_NONCOPYABLE(DBChunk)

public:
	DBChunk() = default;
	~DBChunk();

	// Возвращает путь к файлу БД. Если путь не был
	// назначен, то функция вернёт пустую строку
	std::wstring GetFilePath() const;
	// Назначает path как новый путь к файлу БД
	void SetFilePath(const std::wstring& path);

	using State = DBChunkState;
	using Flag = DBChunkFlags::Flag;
	State GetDataState() const { return m_DataState; }
	State GetSaveState() const { return m_SaveState; }

	void SetHasOwner() { m_Flags.Set(Flag::HAS_OWNER); }
	bool HasOwner() const { return m_Flags.Check(Flag::HAS_OWNER); }
	bool HasOldFormat() const { return m_Flags.Check(Flag::OLD_FORMAT_VER); }

	// Инициализирует объект для накопления данных и последующего сохранения в новый файл. Параметр
	// first задаёт первое число, которое будет проверено в формируемом интервале. Второй параметр
	// reserveCount позволяет сразу зарезервировать память под указанное количество элементов,
	// чтобы избежать потом ненужных ресайзов контейнера (вектора) при добавлении палиндромов
	void Init(const Number& first, size_t reserveCount = 0);

	// Загружает указанное количество данных из файла. Если данные уже загружены, то ничего не делает.
	// Если при загрузке произойдёт ошибка, то функция вернёт false, а состояние объекта не изменится
	bool LoadData(DataBase& db, State stateNeeded);
	// Выгружает данные из памяти, оставляя только указанное
	// количество. Объект не должен содержать изменений
	void UnloadData(State stateNeeded);

	// Сохраняет накопленные изменения в файл. Параметр minSavedStep должен содержать значение шага,
	// начиная с которого найденные палиндромы сохранялись. Параметр cpuTime должен быть равен
	// суммарному времени CPU (в ms), которое было затрачено на проверку добавленных данных
	bool Save(DataBase& db, unsigned minSavedStep, unsigned cpuTime);

	unsigned GetFormatVer() const { return GetData(State::HEADERONLY)->GetFormatVer(); }
	const FixNumber& GetFirst() const { CheckState(State::DATAUNLOADED); return m_First; }
	const Number& GetLast() const { return GetData(State::HEADERONLY)->GetLast(); }
	void SetLast(const Number& num) { GetData(State::HEADERONLY)->SetLast(num); }

	uint64_t GetIterationC() const { return GetData(State::HEADERONLY)->GetIterationC(); }
	uint64_t GetPrimaryLychrelC() const { return GetData(State::HEADERONLY)->GetPrimaryLychrelC(); }
	const Number& GetAllLychrelC() const { return GetData(State::HEADERONLY)->GetAllLychrelC(); }
	uint64_t GetSavedPalindromeC() const { return GetData(State::HEADERONLY)->GetSavedPalindromeC(); }
	const Number& GetAllSavedPalindromeC() const { return GetData(State::HEADERONLY)->GetAllSavedPalindromeC(); }
	unsigned GetCPUTimeSpent() const { return GetData(State::HEADERONLY)->GetCPUTimeSpent(); }
	unsigned GetMinSavedStep() const { return GetData(State::HEADERONLY)->GetMinSavedStep(); }
	unsigned GetSearchDepth() const { return GetData(State::HEADERONLY)->GetSearchDepth(); }

	// Возвращает размер несжатых/сжатых данных/полный размер файла. Значения актуальны,
	// только если данные не менялись после загрузки БД или последнего сохранения файла
	unsigned GetDataSize() const { return GetData(State::HEADERONLY)->GetDataSize(); }
	unsigned GetCDataSize() const { return GetData(State::HEADERONLY)->GetCDataSize(); }
	unsigned GetFileSize() const { return GetData(State::HEADERONLY)->GetFileSize(); }

	const unsigned* GetNumCountA() const { return GetData(State::WITHSTATS)->GetNumCountA(); }
	unsigned GetHighestStep() const { return GetData(State::WITHSTATS)->GetHighestStep(); }
	// Вычисляет полное количество сохранённых чисел в файле (на основе блока статистики)
	size_t GetTotalNumberC() const { return GetData(State::WITHSTATS)->GetTotalNumberC(); }

	using DataItems = DBChunkData::DataItems;
	const DataItems& GetNumbers() const { return GetData(State::FULLDATA)->GetNumbers(); }
	// Сортирует числа в DataItems по возрастанию
	void SortNumbers() { GetData(State::FULLDATA)->SortNumbers(); }

	// Добавляет отложенный палиндром (см. комментарий в DBChunkData)
	void AddPalindrome() { GetData(State::FULLDATA)->AddPalindrome(); }
	void AddPalindrome(const Number& num, unsigned step) { GetData(State::FULLDATA)->AddPalindrome(num, step); }
	// Обновляет статистику по числам Лишрел (см. комментарий в DBChunkData)
	void AddLychrel(const Number& num, unsigned stepC) { GetData(State::FULLDATA)->AddLychrel(num, stepC); }

	// Удаляет из блока данных все сохранённые палиндромы с шагом ниже minStep и корректирует
	// статистику. Функция возвращает true, если был удалён хотя бы один отложенный палиндром
	bool RemovePalindromes(unsigned minStep) { return GetData(State::FULLDATA)->RemovePalindromes(minStep); }

private:
	// См. аналогичный комментарий в классе DBChunkData
	using EE = AssertHelper<EDBLogic>;

	void CheckState(State s) const { if (s > m_DataState) OnNoData(s); }
	DBChunkData* GetData(State s) const { CheckState(s); return m_pData; }
	void OnNoData(State dataNeeded) const;
	void CreateData();

	DBChunkData* m_pData = nullptr;				// Указатель на объект данных или nullptr
	State m_DataState = State::UNINITIALIZED;	// Состояние загрузки данных из файла в объект
	State m_SaveState = State::UNCHANGED;		// Состояние изменений (необходимости сохранения)
	DBChunkFlags m_Flags = 0;					// Различные битовые флаги (см. DBChunkFlags)
	char m_FilePathA[12];						// Путь (2 символа) + имя файла (10 символов)
	FixNumber m_First;							// Первое число проверенного интервала
};

//----------------------------------------------------------------------------------------------------------------------
class DBChunkAccessor final
{
public:
	DBChunkAccessor() = delete;
	const DBChunk* operator ->() const { return reinterpret_cast<const DBChunk*>(this); }

	void SetDataState(DBChunkState state) { reinterpret_cast<DBChunk*>(this)->m_DataState = state; }
	void SetSaveState(DBChunkState state) { reinterpret_cast<DBChunk*>(this)->m_SaveState = state; }

	void RaiseSaveState(DBChunkState state)
	{
		DBChunkState& saveState = reinterpret_cast<DBChunk*>(this)->m_SaveState;
		saveState = (state > saveState) ? state : saveState;
	}

	DBChunkFlags& Flags() { return reinterpret_cast<DBChunk*>(this)->m_Flags; }
	FixNumber& First() { return reinterpret_cast<DBChunk*>(this)->m_First; }
};
