//∙MDPN
#include "pch.h"
#include "dbchunk.h"

#include "dbase.h"
#include "dbstruct.h"
#include "util.h"

#include <core/array.h>
#include <core/crc32.h>
#include <core/datetime.h>
#include <core/exception.h>
#include <core/file.h>
#include <core/strutil.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Util - вспомогательные функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct Util
{
	static char LoCase(char c);

	static bool AToCRC(uint32_t& out, const char* pData);
	static bool AToInt(unsigned& out, const char* pNum, size_t len);
	static bool AToI64(uint64_t& out, const char* pNum, size_t len);
	static bool AToNumber(FixNumber& out, const char* pNum, size_t len);

	static bool SafeAToInt(unsigned& out, const char* pNum, size_t len);
	static bool SafeAToNumber(Number& out, const char* pNum, size_t len);

private:
	static bool IsGreater(const char* pLhs, size_t lhsLen, const char* pRhs, size_t rhsLen);
};

//----------------------------------------------------------------------------------------------------------------------
inline char Util::LoCase(char c)
{
	return (c >= 'A' && c <= 'Z') ?
		c + 32 : c;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool Util::AToCRC(uint32_t& out, const char* pData)
{
	uint32_t n = 0;
	for (size_t i = 0; i < 8; ++i)
	{
		n <<= 4;
		const char c = pData[i];
		if (c >= '0' && c <= '9')
			n += (c - 48) & 0xf;
		else if (c >= 'A' && c <= 'F')
			n += (c - 55) & 0xf;
		else if (c >= 'a' && c <= 'f')
			n += (c - 87) & 0xf;
		else
			return false;
	}
	out = n;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool Util::AToInt(unsigned& out, const char* pNum, size_t len)
{
	while (len > 1 && *pNum == '0')
		++pNum, --len;

	if (!len || IsGreater(pNum, len, "4294967295", 10))
		return false;

	size_t sum = 0;
	for (size_t i = 0; i < len; ++i)
	{
		const uint8_t c = pNum[i];
		if (c < '0' || c > '9')
			return false;
		sum = sum * 10 + c - '0';
	}

	out = static_cast<unsigned>(sum);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool Util::AToI64(uint64_t& out, const char* pNum, size_t len)
{
	while (len > 1 && *pNum == '0')
		++pNum, --len;

	if (!len || IsGreater(pNum, len, "18446744073709551615", 20))
		return false;

	uint64_t sum = 0;
	for (size_t i = 0; i < len; ++i)
	{
		const uint8_t c = pNum[i];
		if (c < '0' || c > '9')
			return false;
		sum = sum * 10 + c - '0';
	}

	out = sum;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool Util::AToNumber(FixNumber& out, const char* pNum, size_t len)
{
	constexpr size_t MAX_LENGTH = 39;
	if (!len || len > MAX_LENGTH)
		return false;

	char buffer[MAX_LENGTH + 1];
	for (size_t i = 0; i < len; ++i)
	{
		const char c = pNum[i];
		if (c < '0' || c > '9')
			return false;
		buffer[i] = c;
	}
	buffer[len] = 0;

	out = buffer;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool Util::SafeAToInt(unsigned& out, const char* pNum, size_t len)
{
	while (len > 1 && *pNum == '0')
		++pNum, --len;

	if (!len || IsGreater(pNum, len, "4294967295", 10))
		return false;

	size_t sum = 0;
	for (size_t i = 0; i < len; ++i)
		sum = sum * 10 + pNum[i] - '0';

	out = static_cast<unsigned>(sum);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool Util::SafeAToNumber(Number& out, const char* pNum, size_t len)
{
	constexpr size_t MAX_LENGTH = 39;
	if (!len || len > MAX_LENGTH)
		return false;

	char buffer[MAX_LENGTH + 1];
	for (size_t i = 0; i < len; ++i)
		buffer[i] = pNum[i];
	buffer[len] = 0;

	out = buffer;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool Util::IsGreater(const char* pLhs, size_t lhsLen, const char* pRhs, size_t rhsLen)
{
	if (lhsLen != rhsLen)
		return lhsLen > rhsLen;

	for (size_t i = 0; i < rhsLen; ++i)
	{
		if (int dif = pLhs[i] - pRhs[i])
			return dif > 0;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileHeader - заголовок файла БД
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct FileHeaderBase
{
	uint32_t headerCRC = 0;		// "%08X" - CRC32 заголовка без первых 8 байт (FILE_HEADER_SIZE - 8 байт)
	unsigned formatVer = 0;		// "FORMAT:" - версия формата (всегда самое первое поле заголовка после CRC)

	// Проверяет CRC заголовка и инициализирует значения полей headerCRC и formatVer
	bool Load(const char* pData);
};

//----------------------------------------------------------------------------------------------------------------------
struct FileHeaderV5 final : public FileHeaderBase
{
	FixNumber first;			// Первое число проверенного интервала
	FixNumber last;				// Последнее число проверенного интервала

	uint64_t iterationC = 0;	// Количество проверенных чисел (итераций) в интервале
	uint64_t primLychrelC = 0;	// Количество первичных чисел Лишрел, найденных в интервале
	uint64_t savedPalC = 0;		// Количество отложенных палиндромов, сохранённых в блоке данных
	FixNumber allLychrelC;		// Полное количество чисел Лишрел, найденных в интервале
	FixNumber allSavedPalC;		// Полное кол-во отложенных палиндромов, сохранённых в блоке данных
	unsigned CPUTimeSpent = 0;	// Количество времени CPU в ms, потраченного на поиск в интервале
	unsigned minSavedStep = 0;	// Минимальный шаг, начиная с которого сохраняются палиндромы
	unsigned searchDepth = 0;	// Минимальная глубина поиска (проверки кандидатов в числа Лишрел)

	unsigned statSize = 0;		// Размер блока статистики в байтах (расположен сразу за заголовком)
	uint32_t statCRC = 0;		// CRC32 блока статистики (statSize байт, начиная с позиции FILE_HEADER_SIZE)

	unsigned dataSize = 0;		// Размер несжатого блока данных в байтах
	uint32_t dataCRC = 0;		// CRC несжатого блока данных (dataSize байт)
	unsigned zippedSize = 0;	// Размер сжатого блока данных (расположен сразу за блоком статистики)
	bool maxCompressed = false;	// true, если блок данных максимально сжат

	FileHeaderV5(const FileHeaderBase& header);
	// Инициализирует значения всех полей
	bool Load(const char* pData);
};

//----------------------------------------------------------------------------------------------------------------------
bool FileHeaderBase::Load(const char* pData)
{
	if (Util::AToCRC(headerCRC, pData))
	{
		const char* p = pData + 8;
		for (size_t len, next; *p; p += next)
		{
			for (len = 0; p[len] && p[len] != 0x0a && p[len] != 0x0d; ++len);
			for (next = len; p[next] == 0x0a || p[next] == 0x0d; ++next);

			if (len >= 7 && !util::StrNInsCmp(p, "format:", 7))
			{
				if (Util::AToInt(formatVer, p + 7, len - 7))
				{
					// Так как размер заголовка в 3-й версии формата отличается от других версий,
					// то проверить CRC заголовка можно только после прочтения номера его версии
					size_t headerSize = (formatVer <= 3) ? 512 : DBChunkData::FILE_HEADER_SIZE;
					return hash::GetCRC32(pData + 8, headerSize - 8) == headerCRC;
				}
				break;
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
FileHeaderV5::FileHeaderV5(const FileHeaderBase& header)
{
	headerCRC = header.headerCRC;
	formatVer = header.formatVer;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileHeaderV5::Load(const char* pData)
{
	const char* p = pData + 8;
	// NB: символ # используется в заголовке только в качестве заполнителя неиспользованного пространства
	// в конце, поэтому его обнаружение в начале строки можно трактовать как сигнал к завершению парсинга
	for (size_t len, next; *p && *p != '#'; p += next)
	{
		for (len = 0; p[len] && p[len] != 0x0a && p[len] != 0x0d; ++len);
		for (next = len; p[next] == 0x0a || p[next] == 0x0d; ++next);

		switch (Util::LoCase(*p))
		{
			case 'a':
				if (!util::StrNInsCmp(p, "alych:", 6))
				{
					if (!Util::AToNumber(allLychrelC, p + 6, len - 6))
						return false;
				}
				else if (!util::StrNInsCmp(p, "aspal:", 6))
				{
					if (!Util::AToNumber(allSavedPalC, p + 6, len - 6))
						return false;
				}
				break;
			case 'c':
				if (!util::StrNInsCmp(p, "cputime:", 8))
				{
					if (!Util::AToInt(CPUTimeSpent, p + 8, len - 8))
						return false;
				}
				// В версиях формата ниже 5-й поле "CPUTIME" называлось "CPUSPENT"
				else if (!util::StrNInsCmp(p, "cpuspent:", 9))
				{
					if (!Util::AToInt(CPUTimeSpent, p + 9, len - 9))
						return false;
				}
				else if (!util::StrNInsCmp(p, "csize:", 6))
				{
					if (!Util::AToInt(zippedSize, p + 6, len - 6))
						return false;
					maxCompressed = !zippedSize || p[6] != '0';
				}
				break;
			case 'd':
				if (!util::StrNInsCmp(p, "depth:", 6))
				{
					if (!Util::AToInt(searchDepth, p + 6, len - 6))
						return false;
				}
				else if (!util::StrNInsCmp(p, "dcrc:", 5))
				{
					if (!Util::AToCRC(dataCRC, p + 5))
						return false;
				}
				else if (!util::StrNInsCmp(p, "dsize:", 6))
				{
					if (!Util::AToInt(dataSize, p + 6, len - 6))
						return false;
				}
				break;
			case 'f':
				if (!util::StrNInsCmp(p, "first:", 6))
				{
					if (!Util::AToNumber(first, p + 6, len - 6))
						return false;
				}
				break;
			case 'i':
				if (!util::StrNInsCmp(p, "iter:", 5))
				{
					if (!Util::AToI64(iterationC, p + 5, len - 5))
						return false;
				}
				break;
			case 'l':
				if (!util::StrNInsCmp(p, "last:", 5))
				{
					if (!Util::AToNumber(last, p + 5, len - 5))
						return false;
				}
				else if (!util::StrNInsCmp(p, "lych:", 5))
				{
					if (!Util::AToI64(primLychrelC, p + 5, len - 5))
						return false;
				}
				break;
			case 'm':
				if (!util::StrNInsCmp(p, "minstep:", 8))
				{
					if (!Util::AToInt(minSavedStep, p + 8, len - 8))
						return false;
				}
				break;
			case 's':
				if (!util::StrNInsCmp(p, "scrc:", 5))
				{
					if (!Util::AToCRC(statCRC, p + 5))
						return false;
				}
				else if (!util::StrNInsCmp(p, "spal:", 5))
				{
					if (!Util::AToI64(savedPalC, p + 5, len - 5))
						return false;
				}
				else if (!util::StrNInsCmp(p, "ssize:", 6))
				{
					if (!Util::AToInt(statSize, p + 6, len - 6))
						return false;
				}
				break;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunkData
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
DBChunkData::DBChunkData(DBChunkAccessor& owner)
	: m_Chunk(owner)
{
}

//----------------------------------------------------------------------------------------------------------------------
DBChunkData::~DBChunkData()
{
	AML_SAFE_DELETE(m_pData);
	AML_SAFE_DELETEA(m_NumCountA);
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::Init(size_t reserveCount)
{
	Assert(!m_NumCountA && !m_pData);

	m_NumCountA = new unsigned[Const::MAX_STEP + 1];
	AML_FILLA(m_NumCountA, 0, Const::MAX_STEP + 1);
	m_HighestStep = 0;

	m_pData = new DataItems;
	// NB: резервируем не менее 10K элементов (около 200 KiB), чтобы
	// избежать ресайзов массива контейнера при добавлении элементов
	m_pData->reserve(std::max(reserveCount, size_t(10000)));

	m_Chunk.SetDataState(State::FULLDATA);
	m_Chunk.Flags().Set(Flag::DATA_SORTED);
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::LoadData(util::File& file, State stateNeeded)
{
	Assert(stateNeeded >= State::HEADERONLY);
	if (stateNeeded <= m_Chunk->GetDataState())
		return true;

	Assert(m_Chunk->GetSaveState() == State::UNCHANGED);

	m_FormatVer = 0;
	if (!ReloadHeader(file) || !m_FormatVer)
	{
		if (m_FormatVer > LATEST_FORMAT_VERSION)
			throw util::ERuntime("Database version not supported");
		return false;
	}
	if (stateNeeded >= State::WITHSTATS && m_Chunk->GetDataState() < State::WITHSTATS)
	{
		if (!LoadStatBlock(file))
			return false;
	}
	if (stateNeeded >= State::FULLDATA && m_Chunk->GetDataState() < State::FULLDATA)
	{
		if (!LoadDataBlock(file))
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::UnloadData(State stateNeeded)
{
	if (stateNeeded < State::FULLDATA)
	{
		AML_SAFE_DELETE(m_pData);
		m_Chunk.Flags().Clear(Flag::DATA_SORTED);
	}
	if (stateNeeded < State::WITHSTATS)
	{
		AML_SAFE_DELETEA(m_NumCountA);
		m_HighestStep = 0;
	}

	Assert(stateNeeded >= State::HEADERONLY);
	m_Chunk.SetDataState(stateNeeded);
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::Save(util::File& file, bool forceFullSave, bool maxCompression)
{
	std::string stats;
	util::MemoryFile numData;
	bool didFullSave = false;

	forceFullSave |= m_Chunk->HasOldFormat() || (maxCompression && !m_MaxCompressed);
	if (m_Chunk->GetSaveState() >= State::DATACHANGED || forceFullSave)
	{
		SaveStats(stats);
		m_StatSize = static_cast<unsigned>(stats.size());
		m_StatCRC = m_StatSize ? hash::GetCRC32(stats.c_str(), m_StatSize) : 0;
		m_DataSize = m_DataCRC = m_CDataSize = 0;
		m_MaxCompressed = false;

		if (numData.Open() && SaveData(numData) && numData.GetSize() >= 0)
		{
			m_DataSize = static_cast<unsigned>(numData.GetSize());
			m_MaxCompressed = didFullSave = !m_DataSize;
			if (m_DataSize)
			{
				util::MemoryFile packedData;
				if (packedData.Open() && numData.GetCRC32(m_DataCRC) && numData.SetPosition(0) &&
					CompressFile(numData, packedData, maxCompression ? 9 : 5) && packedData.GetSize() > 0)
				{
					m_CDataSize = static_cast<unsigned>(packedData.GetSize());
					m_MaxCompressed = maxCompression;
					numData = std::move(packedData);
					didFullSave = true;
				}
			}
		}
		if (!didFullSave)
			throw util::ERuntime("Unexpected error");
	}

	if (SaveHeader(file) && (!didFullSave ||
		((!m_StatSize || file.Write(stats.c_str(), m_StatSize)) &&
		(!m_CDataSize || numData.SaveTo(file)) && (file.Truncate(), true))))
	{
		m_FormatVer = LATEST_FORMAT_VERSION;
		m_Chunk.Flags().Clear(Flag::OLD_FORMAT_VER | Flag::IS_NEW_CHUNK);
		m_Chunk.SetSaveState(State::UNCHANGED);
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::SetLast(const Number& num)
{
	if (EE::Verify(num >= m_Last, "Incorrect value"))
	{
		m_Last = num;
		m_Chunk.RaiseSaveState(State::HEADERCHANGED);
	}
}

//----------------------------------------------------------------------------------------------------------------------
const Number& DBChunkData::GetAllLychrelC()
{
	if (m_AllLychrelIntC)
	{
		m_AllLychrelNumC += m_AllLychrelIntC;
		m_AllLychrelIntC = 0;
	}
	return m_AllLychrelNumC;
}

//----------------------------------------------------------------------------------------------------------------------
const Number& DBChunkData::GetAllSavedPalindromeC()
{
	if (m_AllSavedPalIntC)
	{
		m_AllSavedPalNumC += m_AllSavedPalIntC;
		m_AllSavedPalIntC = 0;
	}
	return m_AllSavedPalNumC;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBChunkData::GetFileSize() const
{
	const size_t headerSize = (m_FormatVer <= 3) ? 512 : FILE_HEADER_SIZE;
	return static_cast<unsigned>(headerSize) + m_StatSize + m_CDataSize;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::SetCPUTimeSpent(unsigned cpuTime)
{
	if (cpuTime)
	{
		m_CPUTimeSpent += cpuTime;
		m_Chunk.RaiseSaveState(State::HEADERCHANGED);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::SetMinSavedStep(unsigned minSavedStep)
{
	if (minSavedStep && minSavedStep < m_MinSavedStep)
	{
		m_MinSavedStep = minSavedStep;
		m_Chunk.RaiseSaveState(State::HEADERCHANGED);
	}
}

//----------------------------------------------------------------------------------------------------------------------
size_t DBChunkData::GetTotalNumberC() const
{
	Assert(m_NumCountA);

	size_t totalC = 0;
	for (unsigned i = 1; i <= m_HighestStep; ++i)
		totalC += m_NumCountA[i];
	return totalC;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::SortNumbers()
{
	Assert(m_pData);
	if (!m_Chunk.Flags().Check(Flag::DATA_SORTED))
	{
		std::sort(m_pData->begin(), m_pData->end());
		m_Chunk.Flags().Set(Flag::DATA_SORTED);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::AddPalindrome(const Number& num, unsigned step)
{
	Assert(m_NumCountA && m_pData);
	EE::Assert(step && step <= Const::MAX_STEP, "Invalid step value");

	if (num > m_Last)
	{
		m_Last = num;
		++m_IterationC;
	}
	++m_SavedPalC;
	uint64_t allSavedPalIntC = m_AllSavedPalIntC;
	m_AllSavedPalIntC = allSavedPalIntC += 1 + num.GetKinNumberCount();
	// NB: при длине числа до 30 знаков включительно макс. значение, которое может быть добавлено
	// к переменной *IntC, равно 9*10^14. Чтобы не прибавлять *IntC к *NumC слишком часто (это долго),
	// мы хотим накапливать в *IntC как можно большее значение. Но оно всегда должно оставаться таким,
	// чтобы можно было увеличить его ещё один раз и не получить при этом переполнения
	static_assert(Const::MAX_DIGIT_C <= 30, "MAX_INTC_VALUE must be updated");
	constexpr uint64_t MAX_INTC_VALUE = ~0ull - 900000000000000ull;
	if (allSavedPalIntC > MAX_INTC_VALUE)
	{
		m_AllSavedPalNumC += allSavedPalIntC;
		m_AllSavedPalIntC = 0;
	}

	++m_NumCountA[step];
	if (!m_MinSavedStep || step < m_MinSavedStep)
		m_MinSavedStep = step;
	m_HighestStep = (step > m_HighestStep) ? step : m_HighestStep;

	DataItem item { num, step };
	if (m_Chunk.Flags().Check(Flag::DATA_SORTED))
	{
		if (!m_pData->empty() && item.num < m_pData->back().num)
			m_Chunk.Flags().Clear(Flag::DATA_SORTED);
	}
	m_pData->push_back(item);

	m_Chunk.RaiseSaveState(State::DATACHANGED);
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::AddLychrel(const Number& num, unsigned stepC)
{
	EE::Assert(num > m_Last && stepC, "Invalid Lychrel number");

	m_Last = num;
	++m_IterationC;
	++m_PrimaryLychrelC;

	uint64_t allLychrelIntC = m_AllLychrelIntC;
	m_AllLychrelIntC = allLychrelIntC += 1 + num.GetKinNumberCount();
	constexpr uint64_t MAX_INTC_VALUE = ~0ull - 900000000000000ull;
	if (allLychrelIntC > MAX_INTC_VALUE)
	{
		m_AllLychrelNumC += allLychrelIntC;
		m_AllLychrelIntC = 0;
	}

	if (!m_SearchDepth || stepC < m_SearchDepth)
		m_SearchDepth = stepC;

	m_Chunk.RaiseSaveState(State::HEADERCHANGED);
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::RemovePalindromes(unsigned minStep)
{
	Assert(m_NumCountA && m_pData);
	EE::Assert(minStep && minStep <= Const::MAX_STEP && minStep >= m_MinSavedStep,
		"Invalid minStep value");

	size_t removedC = 0;
	DataItems& numbers = *m_pData;
	const size_t count = numbers.size();
	for (size_t i = 0; i < count; ++i)
	{
		DataItem& item = numbers[i];
		if (item.step < minStep)
			++removedC;
		else if (removedC)
			numbers[i - removedC] = item;
	}

	if (!removedC)
		return false;

	numbers.resize(count - removedC);
	m_SavedPalC = numbers.size();

	m_AllSavedPalIntC = 0;
	m_AllSavedPalNumC.SetZero();
	m_MinSavedStep = minStep;

	AML_FILLA(m_NumCountA, 0, Const::MAX_STEP + 1);
	m_HighestStep = 0;

	Number num;
	for (const auto& item : *m_pData)
	{
		num = item.num;
		m_AllSavedPalNumC += 1 + num.GetKinNumberCount();

		++m_NumCountA[item.step];
		m_HighestStep = (item.step > m_HighestStep) ? item.step : m_HighestStep;
	}

	m_Chunk.RaiseSaveState(State::DATACHANGED);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::Append(const DBChunkData* pOther)
{
	Assert(m_NumCountA && m_pData);
	Assert(pOther && pOther->m_NumCountA && pOther->m_pData);
	// Проверим, что между началом второго файла и концом первого нет разрыва, а так же
	// что второй файл был проинициализирован и содержит как минимум одно проверенное число
	EE::Assert(m_Last + 1u == pOther->m_Chunk.First() && pOther->m_Chunk.First() <= pOther->m_Last,
		"DBChunkData objects can't be combined together");

	m_Last = pOther->m_Last;
	m_IterationC += pOther->m_IterationC;
	m_PrimaryLychrelC += pOther->m_PrimaryLychrelC;
	m_SavedPalC += pOther->m_SavedPalC;
	m_AllLychrelIntC += pOther->m_AllLychrelIntC;
	m_AllLychrelNumC += pOther->m_AllLychrelNumC;
	m_AllSavedPalIntC += pOther->m_AllSavedPalIntC;
	m_AllSavedPalNumC += pOther->m_AllSavedPalNumC;
	m_CPUTimeSpent += pOther->m_CPUTimeSpent;
	m_MinSavedStep = std::min(m_MinSavedStep, pOther->m_MinSavedStep);
	m_SearchDepth = std::min(m_SearchDepth, pOther->m_SearchDepth);
	m_HighestStep = std::max(m_HighestStep, pOther->m_HighestStep);
	m_Chunk.RaiseSaveState(State::HEADERCHANGED);

	if (const size_t count = pOther->m_pData->size())
	{
		for (unsigned i = 1; i < Const::MAX_STEP; ++i)
			m_NumCountA[i] += pOther->m_NumCountA[i];

		m_pData->reserve(m_pData->size() + count);
		for (const auto& item : *pOther->m_pData)
			m_pData->push_back(item);

		m_Chunk.RaiseSaveState(State::DATACHANGED);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::ReloadHeader(util::File& file)
{
	// NB: buffer должен быть null-terminated строкой, чтобы при парсинге заголовка мы не вышли за пределы
	// массива. Включительно до 3-й версии формата размер заголовка был равен 512 байтам, а начиная с 4-й
	// версии был уменьшен до 400 байт. Размер буфера должен быть достаточным для всех актуальных версий
	constexpr size_t MAX_HEADER_SIZE = std::max(size_t(512), FILE_HEADER_SIZE);
	char buffer[MAX_HEADER_SIZE + 1];
	buffer[MAX_HEADER_SIZE] = 0;

	size_t bytesRead;
	FileHeaderBase ver;
	if (!file.Read(buffer, MAX_HEADER_SIZE, bytesRead) || bytesRead < FILE_HEADER_SIZE || !ver.Load(buffer))
		return false;

	// Если объект уже содержит загруженные данные, то сохранённый CRC должен совпадать с
	// только что прочитанным CRC из файла, чтобы гарантировать то, что файл не изменился
	if (m_Chunk->GetDataState() >= State::HEADERONLY && m_HeaderCRC != ver.headerCRC)
		return false;

	m_FormatVer = ver.formatVer;
	if (ver.formatVer < LATEST_FORMAT_VERSION)
		m_Chunk.Flags().Set(Flag::OLD_FORMAT_VER);

	if (ver.formatVer < 3 || ver.formatVer > 5)
		return false;

	if (m_Chunk->GetDataState() < State::HEADERONLY)
	{
		FileHeaderV5 header(ver);
		if (!header.Load(buffer))
			return false;

		// Значение first не может быть равно 0. Задаётся оно только один раз и никогда потом не меняется,
		// поэтому если в объекте значение задано, то оно обязано совпадать со значением в заголовке файла
		if (!header.first || (m_Chunk.First() && m_Chunk.First() != header.first))
			return false;
		// Здесь выполняем только минимальную проверку корректности данных (только то,
		// что повлияет на обработку непроверенных/накладывающихся интервалов чисел)
		if (header.first > header.last)
			return false;

		m_HeaderCRC = header.headerCRC;
		m_Chunk.First() = header.first;
		m_Last = header.last;
		m_IterationC = header.iterationC;
		m_PrimaryLychrelC = header.primLychrelC;
		m_SavedPalC = header.savedPalC;
		m_AllLychrelIntC = 0;
		m_AllLychrelNumC = header.allLychrelC;
		m_AllSavedPalIntC = 0;
		m_AllSavedPalNumC = header.allSavedPalC;
		m_CPUTimeSpent = header.CPUTimeSpent;
		m_MinSavedStep = header.minSavedStep;
		m_SearchDepth = header.searchDepth;

		m_StatSize = header.statSize;
		m_StatCRC = header.statCRC;

		m_DataSize = header.dataSize;
		m_DataCRC = header.dataCRC;
		m_CDataSize = header.zippedSize;
		m_MaxCompressed = header.maxCompressed;

		m_Chunk.SetDataState(State::HEADERONLY);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::LoadStatBlock(util::File& file)
{
	Assert(!m_NumCountA && m_Chunk->GetDataState() == State::HEADERONLY);

	m_NumCountA = new unsigned[Const::MAX_STEP + 1];
	AML_FILLA(m_NumCountA, 0, Const::MAX_STEP + 1);
	m_HighestStep = 0;

	bool ok = false;
	if (!m_StatSize)
		ok = true;
	else
	{
		util::SmartArray<char, 1024> buffer(m_StatSize + 1);
		size_t headerSize = (m_FormatVer <= 3) ? 512 : FILE_HEADER_SIZE;
		if (file.SetPosition(headerSize) && file.Read(buffer, m_StatSize))
		{
			if (hash::GetCRC32(buffer, m_StatSize) == m_StatCRC)
			{
				buffer[m_StatSize] = 0;
				ok = ParseStats(buffer);
			}
		}
	}

	if (ok)
	{
		m_Chunk.SetDataState(State::WITHSTATS);
		return true;
	}
	AML_SAFE_DELETEA(m_NumCountA);
	m_HighestStep = 0;
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::LoadDataBlock(util::File& file)
{
	Assert(!m_pData && m_Chunk->GetDataState() == State::WITHSTATS);

	m_pData = new DataItems;

	bool ok = false;
	if (!m_CDataSize)
		ok = !m_DataSize;
	else
	{
		size_t headerSize = (m_FormatVer <= 3) ? 512 : FILE_HEADER_SIZE;
		const long long fileOffset = headerSize + m_StatSize;
		if (file.GetSize() >= fileOffset + m_CDataSize)
		{
			util::MemoryFile data;
			if (file.SetPosition(fileOffset) && data.Open() &&
				DecompressFile(file, data) && data.GetSize() == m_DataSize)
			{
				util::DynamicArray<char> buffer(m_DataSize + 1);
				if (data.SetPosition(0) && data.Read(buffer, m_DataSize) &&
					hash::GetCRC32(buffer, m_DataSize) == m_DataCRC)
				{
					data.Close();
					m_pData->reserve(GetTotalNumberC());
					buffer[m_DataSize] = 0;
					ok = ParseData(buffer);
				}
			}
		}
	}

	if (ok)
	{
		m_Chunk.SetDataState(State::FULLDATA);
		return true;
	}
	AML_SAFE_DELETE(m_pData);
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::ParseStats(const char* pData)
{
	unsigned step, count;
	const char* p = pData;
	for (size_t len, next, k; *p; p += next)
	{
		for (len = 0; p[len] && p[len] != 0x0a && p[len] != 0x0d; ++len);
		for (next = len; p[next] == 0x0a || p[next] == 0x0d; ++next);

		if (len && *p == '#')
		{
			for (k = 1; k < len && p[k] != '='; ++k);
			if (k <= 1 || k + 1 >= len || !Util::AToInt(step, p + 1, k - 1) || !step ||
				step > Const::MAX_STEP || !Util::AToInt(count, p + k + 1, len - k - 1))
			{
				return false;
			}
			m_NumCountA[step] += count;
			if (count && step > m_HighestStep)
				m_HighestStep = step;
		}
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::ParseData(const char* pData)
{
	DataItem item;
	Number last, num;

	const char* p = pData;
	while (*p == 10) ++p;

	if (m_FormatVer < LATEST_FORMAT_VERSION)
	{
		// Читаем блок данных в старых форматах: 3 и 4
		for (size_t k; *p; p += k)
		{
			for (k = 0; p[k] >= '0' && p[k] <= '9'; ++k);
			if (!k || p[k] != 32 || !Util::SafeAToNumber(num, p, k) || !num)
				return false;

			for (p += k + 1, k = 0; p[k] >= '0' && p[k] <= '9'; ++k);
			if (!k || (p[k] && p[k] != 10) || !Util::SafeAToInt(item.step, p, k) || !item.step)
				return false;

			if (m_FormatVer <= 3)
				item.num = num;
			else
			{
				last += num;
				item.num = last;
			}
			m_pData->push_back(item);
			k += (p[k] == 10) ? 1 : 0;
		}
	} else
	{
		// Читаем блок данных в текущем актуальном (самом
		// последнем) формате LATEST_FORMAT_VERSION
		constexpr size_t BUF_SIZE = 64;
		char buffer[BUF_SIZE];

		for (size_t k; *p; p += k)
		{
			size_t len = 0;
			for (k = 0; p[k] != 32; ++k)
			{
				if (p[k] >= '0' && p[k] <= '9')
				{
					if (len >= BUF_SIZE)
						return false;
					buffer[len++] = p[k];
				}
				else if (p[k] >= 'A' && p[k] <= 'Z')
				{
					static const uint8_t countA[28] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
						12, 13, 14, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
					const size_t sameC = countA[static_cast<size_t>(p[k]) - 'A'];
					if (len + sameC > BUF_SIZE)
						return false;
					const char digit = (p[k] < 'N') ? '0' : '9';
					for (size_t i = 0; i < sameC; ++i)
						buffer[len + i] = digit;
					len += sameC;
				}
				else
					return false;
			}
			if (!len || len >= BUF_SIZE)
				return false;
			buffer[len] = 0;
			num = buffer;

			for (p += k + 1, k = 0; p[k] >= '0' && p[k] <= '9'; ++k);
			if (!k || (p[k] && p[k] != 32) || !Util::SafeAToInt(item.step, p, k))
				return false;

			last += num;
			item.num = last;
			item.step += m_MinSavedStep;
			if (!num || !item.step)
				return false;

			m_pData->push_back(item);
			k += (p[k] == 32) ? 1 : 0;
		}
	}

	// Начиная с 4-й версии формата, мы используем дельта-кодирование, что гарантирует
	// нам неубывающий порядок чисел, т.е. то, что вектор m_pData будет отсортирован.
	// Для предыдущих версий проверим, что числа расположены в неубывающем порядке
	if (m_FormatVer > 3 || std::is_sorted(m_pData->begin(), m_pData->end()))
		m_Chunk.Flags().Set(Flag::DATA_SORTED);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::SaveHeader(util::File& file)
{
	Number first;
	util::DateTime dt;
	std::string header, timeStamp;
	header.reserve(FILE_HEADER_SIZE);

	dt.Update(true);
	timeStamp = util::Format("%02u%02u%02u%02u%02u%02u",
		dt.year % 100, dt.month, dt.day, dt.hour, dt.minute, dt.second);

	first = m_Chunk.First();
	m_AllLychrelNumC += m_AllLychrelIntC;
	m_AllSavedPalNumC += m_AllSavedPalIntC;
	m_AllSavedPalIntC = m_AllLychrelIntC = 0;

	header += util::Format("\nFORMAT:%u\n", LATEST_FORMAT_VERSION);
	header += util::Format("TIME:%s\n", timeStamp.c_str());

	header += util::Format("FIRST:%s\n", first.AsString().c_str());
	header += util::Format("LAST:%s\n", m_Last.AsString().c_str());
	header += util::Format("ITER:%llu\n", m_IterationC);
	header += util::Format("LYCH:%llu\n", m_PrimaryLychrelC);
	header += util::Format("SPAL:%llu\n", m_SavedPalC);
	header += util::Format("ALYCH:%s\n", m_AllLychrelNumC.AsString().c_str());
	header += util::Format("ASPAL:%s\n", m_AllSavedPalNumC.AsString().c_str());
	header += util::Format("CPUTIME:%u\n", m_CPUTimeSpent);
	header += util::Format("MINSTEP:%u\n", m_MinSavedStep);
	header += util::Format("DEPTH:%u\n---\n", m_SearchDepth);

	header += util::Format("SSIZE:%u\nSCRC:%08X\n", m_StatSize, m_StatSize ? m_StatCRC : 0);
	header += util::Format("DSIZE:%u\nDCRC:%08X\n", m_DataSize, m_DataSize ? m_DataCRC : 0);
	header += util::Format((m_MaxCompressed || !m_CDataSize) ? "CSIZE:%u\n" : "CSIZE:0%u\n", m_CDataSize);

	header += "---\n";
	while (header.size() < FILE_HEADER_SIZE - 8)
	{
		if (size_t reminder = (FILE_HEADER_SIZE - 9 - header.size()) % 50)
			header.append(reminder, '#');
		else
			header.back() = '-';
		header += '\n';
	}

	m_HeaderCRC = hash::GetCRC32(header.c_str(), header.size());
	header.insert(0, util::Format("%08X", m_HeaderCRC));

	return file.Write(header.c_str(), header.size());
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkData::SaveStats(std::string& out)
{
	Assert(m_NumCountA);

	out.reserve(1000);
	for (unsigned i = 1; i <= Const::MAX_STEP; ++i)
	{
		if (m_NumCountA[i])
		{
			if (out.empty())
				out += "---\n";
			out += util::Format("#%u=%u\n", i, m_NumCountA[i]);
		}
	}
	if (!out.empty())
		out += "---\n";
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunkData::SaveData(util::File& out)
{
	constexpr size_t BUF_SIZE = 64;
	Assert(m_pData && BUF_SIZE > Const::MAX_DIGIT_C);

	SortNumbers();

	Number last, num;
	char buffer[BUF_SIZE];
	for (const auto& item : *m_pData)
	{
		// Отложенные палиндромы хранятся в блоке данных в текстовом виде. На каждый палиндром
		// по два числа: собственно сам палиндром и количество его шагов, разделённые пробелом.
		// Для значения палиндрома используется дельта-кодирование: самое первое число блока
		// сохраняется как есть, а каждое следующее - как разность между предыдущим
		num = item.num;
		num -= last;

		last = item.num;

		// Получаемые в результате дельта-кодирования числа имеют закономерность: часто они содержат
		// много подряд идущих девяток или нолей. Чтобы дополнительно снизить энтропию и размер сжатых
		// данных, будем кодировать такие последовательности с помощью букв: 13 букв от 'A' до 'M' для
		// последовательностей нолей длиной от 2 до 14 знаков; и аналогично 13 букв от 'N' до 'Z' для
		// кодирования подряд идущих девяток
		num.AsString(buffer, BUF_SIZE);

		char lastDigit = 0;
		size_t sameC = 0, len = 0;
		for (size_t i = 0; buffer[i]; ++i)
		{
			if (buffer[i] == lastDigit && sameC < 14)
			{
				const char base = (lastDigit == '0') ? 'A' - 2 : 'N' - 2;
				buffer[len - 1] = base + static_cast<char>(++sameC);
			} else
			{
				buffer[len++] = buffer[i];
				const size_t j = buffer[i] & 0xff;
				lastDigit = "0########9"[j - '0'];
				sameC = 1;
			}
		}

		if (!out.Write(buffer, len))
			return false;

		Assert(item.step >= m_MinSavedStep);
		// Шаги палиндромов кодируются как разность между значением
		// шага и минимальным сохраняемым шагом палиндромов в файле
		size_t n, k = 20, step = item.step - m_MinSavedStep;
		do {
			n = step;
			step /= 10;
			buffer[k--] = '0' + static_cast<char>(n - 10 * step);
		} while (step);

		buffer[k] = 32;
		buffer[21] = 32;

		if (!out.Write(buffer + k, 22 - k))
			return false;
	}

	// Удаляем концевой пробел
	const auto dataSize = out.GetSize();
	if (dataSize > 0)
	{
		out.SetPosition(out.GetSize() - 1);
		out.Truncate();
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunk
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
DBChunk::~DBChunk()
{
	AML_SAFE_DELETE(m_pData);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring DBChunk::GetFilePath() const
{
	std::wstring path;
	if (m_Flags.Check(Flag::HAS_FILE_PATH))
	{
		static_assert(DBStructure::PATH_LEN == 17, "Wrong path length");
		path.assign(L"12/1234567890.pal");

		path[0] = m_FilePathA[0];
		path[1] = m_FilePathA[1];
		for (size_t i = 2; i < 12; ++i)
			path[i + 1] = m_FilePathA[i];
	}
	return path;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DBChunk::SetFilePath(const std::wstring& path)
{
	Assert(!m_Flags.Check(Flag::HAS_FILE_PATH));
	EE::Assert(DBStructure::IsValidPath(path) && path.size() > sizeof(m_FilePathA),
		"Not a valid path");

	const wchar_t* p = path.c_str();
	m_FilePathA[0] = static_cast<char>(p[0]);
	m_FilePathA[1] = static_cast<char>(p[1]);
	for (size_t i = 2; i < sizeof(m_FilePathA); ++i)
		m_FilePathA[i] = static_cast<char>(p[i + 1]);

	m_Flags.Set(Flag::HAS_FILE_PATH);
	if (m_DataState < State::FILEPATHONLY)
		m_DataState = State::FILEPATHONLY;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunk::Init(const Number& first, size_t reserveCount)
{
	EE::Assert(!first.IsZero(), "Incorrect value");
	EE::Assert(GetDataState() == State::UNINITIALIZED, "Already initialized");

	Assert(!m_Flags.Check(Flag::HAS_FILE_PATH) && !m_First);

	CreateData();
	m_pData->Init(reserveCount);
	// NB: значение m_pData->m_Last остаётся равным 0 до вызова AddPalindome/AddLychrel или SetLast.
	// Если и при сохранении в файл значение будет нулевым, значит имеет место логическая ошибка

	m_SaveState = State::DATACHANGED;
	m_Flags.Set(Flag::IS_NEW_CHUNK);
	m_First = first;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunk::LoadData(DataBase& db, State stateNeeded)
{
	EE::Assert(!m_Flags.Check(Flag::IS_NEW_CHUNK), "Cannot load data");

	if (stateNeeded <= GetDataState())
		return true;

	EE::Assert(GetSaveState() == State::UNCHANGED, "Not saved changes");
	EE::Assert(m_Flags.Check(Flag::HAS_FILE_PATH), "Filepath not set");

	util::BinaryFile file;
	if (!file.Open(db.GetBasePath() + GetFilePath(), util::FILE_OPEN_READ))
		return false;

	if (!m_pData)
		CreateData();

	auto stateToLoad = std::max(stateNeeded, State::HEADERONLY);
	const bool isLoaded = m_pData->LoadData(file, stateToLoad);
	file.Close();

	// Если данные загружены и нам было нужно DATAUNLOADED, то сразу выгрузим заголовок. Нам следует так
	// делать потому, что вызывая LoadData, мы должны знать, что данных загрузится не более, чем указано
	if (isLoaded && stateNeeded < stateToLoad)
		UnloadData(stateNeeded);

	return isLoaded;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunk::UnloadData(State stateNeeded)
{
	if (stateNeeded < State::DATAUNLOADED)
		stateNeeded = State::DATAUNLOADED;

	if (stateNeeded >= GetDataState())
		return;

	// Мы не можем просто выгрузить данные, если есть несохранённые изменения, так как это
	// приведёт к тому, что оставшиеся в объекте данные будут отличаться от данных в файле
	EE::Assert(GetSaveState() == State::UNCHANGED || (GetSaveState() == State::HEADERCHANGED &&
		stateNeeded >= State::HEADERONLY), "Changes not saved");

	if (stateNeeded == State::DATAUNLOADED)
	{
		AML_SAFE_DELETE(m_pData);
		m_DataState = State::DATAUNLOADED;
	} else
	{
		Assert(m_pData);
		m_pData->UnloadData(stateNeeded);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool DBChunk::Save(DataBase& db, unsigned minSavedStep, unsigned cpuTime, bool maxCompression)
{
	if (GetSaveState() == State::UNCHANGED)
	{
		if (maxCompression)
		{
			Assert(GetDataState() >= State::FULLDATA, "Data not loaded");
			if (m_pData->IsMaxCompressed())
				return true;
		}
		return true;
	}

	Assert(m_pData && m_pData->GetLast());
	Assert(m_Flags.Check(Flag::HAS_FILE_PATH));

	m_pData->SetMinSavedStep(minSavedStep);
	m_pData->SetCPUTimeSpent(cpuTime);

	// Если изменились данные или файл имеет старую версию, то перезапишем
	// весь файл целиком. Иначе можно ограничиться лишь сохранением заголовка
	const bool needFullSave = GetSaveState() >= State::DATACHANGED || HasOldFormat() ||
		(maxCompression && !m_pData->IsMaxCompressed());
	unsigned openMode = needFullSave ? util::FILE_CREATE_ALWAYS : util::FILE_OPEN_ALWAYS;

	util::BinaryFile file;
	auto filePath = db.GetBasePath() + GetFilePath();
	if (!file.Open(filePath, util::FILE_OPEN_WRITE | openMode))
		return false;

	bool ok = m_pData->Save(file, false, maxCompression);
	file.Close();
	return ok;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunk::Append(const DBChunk* pOther)
{
	if (pOther)
	{
		const DBChunkData* pOtherData = pOther->GetData(State::FULLDATA);
		GetData(State::FULLDATA)->Append(pOtherData);
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DBChunk::OnNoData(State dataNeeded) const
{
	EE::Assert(dataNeeded <= GetDataState(), "Data not loaded");
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunk::CreateData()
{
	Assert(!m_pData);
	auto pAccessor = reinterpret_cast<DBChunkAccessor*>(this);
	m_pData = new DBChunkData(*pAccessor);
}
