//∙MDPN
#include "pch.h"
#include "chkdbmode.h"

#include "dbase.h"
#include "dbchunk.h"
#include "log.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/console.h>
#include <core/file.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBFileIndex
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
DBFileIndex::DBFileIndex(DataBase& data)
	: m_Data(data)
{
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::Load(const std::wstring& filePath)
{
	for (int i = 0; i < 256; ++i)
		m_Index[i].clear();

	util::BufferedFile file(64);
	if (!filePath.empty() && file.Open(m_Data.GetBasePath() + filePath, util::FILE_OPEN_READ))
	{
		uint32_t fileCRC, crc;
		const long long fileSize = file.GetSize();
		if (fileSize > 8 && file.GetCRC32(fileCRC, 8) && file.SetPosition(0))
		{
			char buffer[8];
			if (file.Read(buffer, 8) && AToCRC(crc, buffer) && fileCRC == crc)
				return LoadIndex(file);
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::Save(const std::wstring& filePath, bool saveUnexistent)
{
	if (filePath.empty())
		return false;

	for (int i = 0; i < 256; ++i)
	{
		for (auto& info : m_Index[i])
			info.isExistent = saveUnexistent;
	}
	m_Data.ForEachChunk([this](const DBChunk* pChunk) {
		FileInfo* pInfo = GetInfo(pChunk->GetFilePath());
		if (pInfo && !pInfo->isRemoved && pChunk->GetDataState() >= DBChunkState::DATAUNLOADED)
		{
			pInfo->range = pChunk->GetFirst().GetLength() & 0xff;
			pInfo->isExistent = true;
		}
		return true;
	});

	const std::wstring path = m_Data.GetBasePath() + filePath;
	const std::wstring tmpPath = path + L".tmp";

	bool savedOk = false;
	util::BufferedFile file(64);
	if (file.Open(tmpPath, util::FILE_OPEN_READWRITE | util::FILE_OPEN_ALWAYS))
	{
		if (file.Write("12345678\n", 9) && SaveIndex(file))
		{
			uint32_t crc = 0;
			if (file.GetCRC32(crc, 8) && file.SetPosition(0))
			{
				char buffer[12];
				sprintf_s(buffer, "%08X", crc);
				savedOk = file.Write(buffer, 8);
			}
		}
		file.Close();
	}

	if (savedOk)
	{
		if (util::FileSystem::FileExists(path) && !util::FileSystem::RemoveFile(path))
			return false;
		return util::FileSystem::Rename(tmpPath, path);
	}
	util::FileSystem::RemoveFile(tmpPath);
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBFileIndex::GetLevel(const std::wstring& filePath)
{
	FileInfo* pInfo = GetInfo(filePath);
	if (pInfo && !pInfo->isRemoved)
	{
		bool fileOk = false;
		uint64_t fileSize, fileTime;
		const auto fullPath = m_Data.GetBasePath() + filePath;
		// Первым делом пытаемся получить размер файла и дату/время его последней модификации.
		// Если файл не существует, то функция вернёт false. И если размер файла не изменился,
		// то сравниваем дату/время его последней модификации или вычисляем и проверяем CRC32
		if (GetOSFileInfo(fullPath, fileSize, fileTime) && fileSize == pInfo->fileSize)
		{
			// Если время последней модификации файла не изменилось (поле pInfo->fileTime будет
			// больше 0, только если мы уже ранее проверили CRC32 файла), то считаем, что это
			// тот же самый файл с тем же содержимым, и поэтому не проверяем его CRC32
			if (pInfo->fileTime && fileTime == pInfo->fileTime)
				fileOk = true;
			else
			{
				util::BinaryFile file;
				if (file.Open(fullPath, util::FILE_OPEN_READ))
				{
					const long long fSize = file.GetSize();
					if (fSize >= 0 && fSize == pInfo->fileSize)
					{
						uint32_t fileCRC = 0;
						if (file.GetCRC32(fileCRC) && fileCRC == pInfo->fileCRC)
						{
							pInfo->fileTime = fileTime;
							fileOk = true;
						}
					}
				}
			}
		}

		if (fileOk)
			return pInfo->level;

		// Файл не существует или его размер/CRC32 изменились. В этом случае помечаем
		// запись как "удалённую", так как теперь файл потребует полной перепроверки
		pInfo->isRemoved = true;
	}

	// NB: значение 0 означает отсутствие уровня проверки: либо в индексе
	// такого файла нет, либо файл добавлен в индекс, но ещё не был проверен
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::SetLevel(const std::wstring& filePath, unsigned level, bool remove)
{
	Assert((level && level <= 255) || remove);
	if (FileInfo* pInfo = GetInfo(filePath, !remove))
	{
		pInfo->level = remove ? 0 : static_cast<uint8_t>(level);
		// Сразу выставляем флаг в true. И если всё будет в
		// порядке, мы сбросим его при завершении операции
		pInfo->isRemoved = true;

		if (!remove)
		{
			uint64_t fileSize, fileTime;
			const auto fullPath = m_Data.GetBasePath() + filePath;
			// Получаем размер файла и дату/время его последней модификации. Если файл
			// не существует, то функция вернёт false, а мы завершимся с ошибкой
			if (!GetOSFileInfo(fullPath, fileSize, fileTime))
				return false;

			// Если размер файла корректен и время последней модификации не изменилось (поле pInfo->fileTime
			// будет больше 0, только если мы уже ранее проверили CRC32 файла), то считаем, что это тот же
			// самый файл с тем же содержимым. Иначе вычисляем CRC32 и обновляем все данные о файле
			if (pInfo->fileTime && fileSize == pInfo->fileSize && fileTime == pInfo->fileTime)
				pInfo->isRemoved = false;
			else
			{
				util::BinaryFile file;
				if (file.Open(fullPath, util::FILE_OPEN_READ))
				{
					uint32_t fileCRC = 0;
					const long long fSize = file.GetSize();
					if (fSize >= 0 && fSize <= 100 * 1024 * 1024 && file.GetCRC32(fileCRC))
					{
						pInfo->fileTime = fileTime;
						pInfo->fileSize = static_cast<unsigned>(fSize);
						pInfo->fileCRC = fileCRC;

						pInfo->range = 0;
						pInfo->isRemoved = false;
					}
				}
				return !pInfo->isRemoved;
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
DBFileIndex::FileInfo* DBFileIndex::GetInfo(const std::wstring& filePath, bool createIfNotFound)
{
	if (!Verify(filePath.size() == DBStructure::PATH_LEN))
		return false;

	char pathA[DBStructure::PATH_LEN + 1];
	const wchar_t* pathWA = filePath.c_str();
	for (size_t i = 0; i < DBStructure::PATH_LEN; ++i)
		pathA[i] = static_cast<char>(pathWA[i]);
	pathA[DBStructure::PATH_LEN] = 0;

	return GetInfo(pathA, createIfNotFound);
}

//----------------------------------------------------------------------------------------------------------------------
DBFileIndex::FileInfo* DBFileIndex::GetInfo(const char* pFilePath, bool createIfNotFound)
{
	const unsigned dir = 16 * ToNumber(pFilePath[0]) + ToNumber(pFilePath[1]);
	FileGroup& group = m_Index[dir];
	intptr_t left = 0, right;

	if (!group.empty())
	{
		right = group.size() - 1;
		while (left <= right)
		{
			const intptr_t mid = (left + right) / 2;
			int v = util::StrCmp(group[mid].filePathA, pFilePath);

			if (v < 0)
				left = mid + 1;
			else if (v > 0)
				right = mid - 1;
			else
				return &group[mid];
		}
	}

	if (createIfNotFound)
	{
		FileInfo info;
		strcpy_s(info.filePathA, pFilePath);
		return &*group.insert(group.begin() + left, info);
	}

	return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::ParseRecord(const std::string& data, unsigned curRange)
{
	const auto elements = util::Split(data, ",;");
	if (elements.size() == 4 && elements[0].size() == DBStructure::PATH_LEN)
	{
		uint32_t fileCRC = 0;
		unsigned fileSize = 0, level = 0;
		if (AToInt(fileSize, elements[1].c_str(), elements[1].size()) && elements[2].size() == 8 &&
			AToCRC(fileCRC, elements[2].c_str()) && AToInt(level, elements[3].c_str(), elements[3].size()))
		{
			if (FileInfo* pInfo = GetInfo(elements[0].c_str(), true))
			{
				pInfo->fileSize = fileSize;
				pInfo->fileCRC = fileCRC;
				pInfo->level = level & 0xff;
				pInfo->range = curRange & 0xff;
				return true;
			}
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::LoadIndex(util::File& file)
{
	constexpr size_t BUF_SIZE = 1024;
	char dataA[BUF_SIZE];
	char* pData = dataA;

	size_t bytesLeft = 0;
	unsigned curRange = 0;
	bool noMoreData = false;

	while (true)
	{
		if (bytesLeft < 120)
		{
			if (!noMoreData)
			{
				size_t bytesRead = 0;
				const size_t bytesToRead = BUF_SIZE - bytesLeft;
				memmove(dataA, pData, bytesLeft);
				if (!file.Read(dataA + bytesLeft, bytesToRead, bytesRead))
					return false;
				bytesLeft += bytesRead;
				noMoreData = bytesRead < bytesToRead;
				pData = dataA;
			}
			else if (!bytesLeft)
				return true;
		}

		size_t k, len = 0;
		const char* p = pData;
		while (len < bytesLeft && p[len] != 10 && p[len] != 13) ++len;
		for (k = len; k < bytesLeft && (p[k] == 10 || p[k] == 13); ++k);
		bytesLeft -= k;
		pData += k;

		if (len)
		{
			if (*p == '#')
			{
				if (!util::StrNInsCmp(p + 1, "RANGE=", 6))
				{
					if (!AToInt(curRange, p + 7, len - 7))
						return false;
				}
			}
			else if (!ParseRecord(std::string(p, len), curRange))
				return false;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::SaveIndex(util::File& file)
{
	std::string out;
	out.reserve(50000);

	unsigned lastRange = 0;
	for (unsigned range = 0; range <= lastRange; ++range)
	{
		size_t fileC = 0;
		out = util::Format(range ? "\n#RANGE=%u\n" : "\n", range);

		for (int dir = 0; dir < 256; ++dir)
		{
			for (const auto& info : m_Index[dir])
			{
				if (info.isExistent && !info.isRemoved)
				{
					lastRange = (lastRange < info.range) ?
						info.range : lastRange;

					if (info.range == range)
					{
						++fileC;
						out += util::Format("%s,%u,%08X,%u\n", info.filePathA,
							info.fileSize, info.fileCRC, info.level);
					}
				}
			}
		}

		if (fileC && !file.Write(out.c_str(), out.size()))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned DBFileIndex::ToNumber(char hexDigit)
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
bool DBFileIndex::AToCRC(uint32_t& out, const char* pData)
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
AML_NOINLINE bool DBFileIndex::AToInt(unsigned& out, const char* pNum, size_t len)
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
bool DBFileIndex::IsGreater(const char* pLhs, size_t lhsLen, const char* pRhs, size_t rhsLen)
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

//----------------------------------------------------------------------------------------------------------------------
bool DBFileIndex::GetOSFileInfo(const std::wstring& filePath, uint64_t& fileSize, uint64_t& lastWriteTime)
{
	WIN32_FILE_ATTRIBUTE_DATA attrData;
	if (::GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &attrData))
	{
		fileSize = (static_cast<uint64_t>(attrData.nFileSizeHigh) << 32) | attrData.nFileSizeLow;
		lastWriteTime = (static_cast<uint64_t>(attrData.ftLastWriteTime.dwHighDateTime) << 32) |
			attrData.ftLastWriteTime.dwLowDateTime;
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CheckDBMode
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
CheckDBMode::CheckDBMode()
	: m_Index(m_Data)
{
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::Run()
{
	if (!m_Executed)
	{
		// Команда "check" - многоуровневая проверка файлов базы данных. Опциональный
		// параметр "--noremove" запрещает автоматическое удаление некорректных файлов
		if (m_Params.size() >= 1 && m_Params.size() <= 2 && !util::StrInsCmp(m_Params[0], "check"))
		{
			bool okToGo = true;
			if (m_Params.size() == 2)
			{
				if (!util::StrInsCmp(m_Params[1], "--noremove"))
					m_DontRemoveBroken = true;
				else
					okToGo = false;
			}

			if (okToGo)
			{
				m_Executed = true;
				return CheckDataBase();
			}
		}

		OnInvalidCmdLine();
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckDataBase()
{
	if (!m_Data.SafeInit())
	{
		aux::Print("Database not found, exiting...\n");
		return false;
	}

	SystemLog::SetPath(m_Data.GetBasePath() + L"log.txt");
	PrintDataBasePath(m_Data.GetBasePath(), 46);

	EventManager::PublishEvent("Database loaded, now loading database index...");

	if (!m_Index.Load() && util::FileSystem::FileExists(m_Data.GetBasePath() + L"index"))
	{
		EventManager::PublishEvent("#12ERROR: failed to load database index");
		SystemLog::Instance().Close();
		return false;
	}

	EventManager::PublishEvent("Database index loaded, now checking files...");
	const float timeInWork = CheckDBChunks(m_Data.GetChunkC());

	if (m_IsIndexChanged)
		m_Index.Save();

	SystemLog::Instance().Close();
	aux::Printf("Time in work: %.1fs\n", timeInWork);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
float CheckDBMode::CheckDBChunks(size_t totalChunkC)
{
	unsigned secondsElapsed = 0;
	uint32_t startTime = ::GetTickCount();
	//uint32_t lastSaveTick = startTime;
	size_t totalFilesProcessedC = 0;
	bool wasAborted = false;

	constexpr unsigned HIGHEST_LEVEL = 6;
	for (unsigned currentLevel = 1; !m_IsCancelled && currentLevel <= HIGHEST_LEVEL; ++currentLevel)
	{
		size_t toProcessC = 0;
		if (totalChunkC)
		{
			size_t chunkC = 0;
			DBProgress onProgress(util::Format("Scanning files (L%u): %%.1f%%%%...", currentLevel));
			m_Data.ForEachChunk([&](const DBChunk* pChunk) {
				if (!(chunkC++ & 0x3f))
				{
					onProgress(100.f * (chunkC - 1) / totalChunkC);
					m_IsCancelled |= util::SystemConsole::Instance().IsCtrlCPressed();
				}
				if (currentLevel == m_Index.GetLevel(pChunk->GetFilePath()) + 1)
					++toProcessC;
				return !m_IsCancelled;
			});
		}

		if (toProcessC && !m_IsCancelled)
		{
			// NB: начиная проверку на уровнях L5 и L6, набор отсева должен быть очищен (он используется
			// только на этих уровнях). Это важно для L6, так как иначе глубокий поиск попросту не будет
			// выполняться из-за добавленых на L5 чисел Лишрел (которые на L5 не проверяются полностью)
			m_LychThreads.Clear(false);

			uint32_t lastTick = 0;
			size_t chunkC = 0, errorC = 0;
			m_Data.ForEachChunk([&](DBChunk* pChunk) {
				const auto filePath = pChunk->GetFilePath();
				if (currentLevel == m_Index.GetLevel(filePath) + 1)
				{
					uint32_t tick = ::GetTickCount();
					if (tick - lastTick >= 500)
					{
						uint32_t seconds = (tick - startTime) / 1000;
						startTime += 1000 * seconds;
						secondsElapsed += seconds;
						lastTick = tick;

						aux::Printf(L"\rL%u -> File #15#%s#7 [%u/%u], %.1f%% done...", currentLevel,
							filePath.c_str(), chunkC + 1, toProcessC, 100.f * chunkC / toProcessC);

						// TODO: так сохранение не работает: внутри m_Index.Save()
						// есть вызов ForEachChunk, что приводит к исключению
						/*if (tick - lastSaveTick >= Const::DATA_SAVE_TIME)
						{
							lastSaveTick = tick;
							if (m_IsIndexChanged)
							{
								m_Index.Save();
								m_IsIndexChanged = false;
							}
						}*/
					}

					bool fileOk = CheckDBChunk(pChunk, currentLevel);
					if (!fileOk)
					{
						++errorC;
						if (!m_DontRemoveBroken)
							util::FileSystem::RemoveFile(m_Data.GetBasePath() + filePath);
					}

					if (!m_IsCancelled)
					{
						++chunkC;
						m_IsCancelled |= util::SystemConsole::Instance().IsCtrlCPressed();
						m_Index.SetLevel(filePath, currentLevel, !fileOk);
						m_IsIndexChanged = true;
					}

					if (errorC >= 30)
					{
						m_IsCancelled = true;
						wasAborted = true;
					}
				}
				return !m_IsCancelled;
			});

			totalFilesProcessedC += chunkC;
			EventManager::PublishEvent(util::Format("Level:%u -> %s of %s file(s) verified, %u error(s) found",
				currentLevel, SeparateWithCommas(chunkC).c_str(), SeparateWithCommas(totalChunkC).c_str(), errorC));
		}

		if (m_IsIndexChanged && !m_IsCancelled)
		{
			m_Index.Save();
			m_IsIndexChanged = false;
		}
	}

	if (wasAborted)
		EventManager::PublishEvent(util::Format("#6Too many errors found, #12aborting..."));
	else if (m_IsCancelled)
		aux::Print("Process was interrupted by user\n");
	else if (!totalFilesProcessedC)
		EventManager::PublishEvent("No unverified files found, exiting...");

	const uint32_t endTime = ::GetTickCount();
	return secondsElapsed + .001f * (endTime - startTime);
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckDBChunk(DBChunk* pChunk, unsigned level)
{
	bool isCorrect = false;

	if (pChunk && level)
	{
		if (!pChunk->LoadData(m_Data, DBChunkState::FULLDATA))
			return OnError(pChunk, 1, "File structure is broken");

		switch (level)
		{
			case 1:
				// Level 1: проверка того, что файл без ошибок загружается
				// кодом БД, поэтому здесь нам больше ничего делать не нужно
				isCorrect = true;
				break;
			case 2:
				isCorrect = CheckLevel2(pChunk);
				break;
			case 3:
				isCorrect = CheckLevel3(pChunk);
				break;
			case 4:
				isCorrect = CheckLevel4(pChunk);
				break;
			case 5:
			case 6:
				isCorrect = CheckLevel5Or6(pChunk, level);
				break;
		}

		pChunk->UnloadData(DBChunkState::DATAUNLOADED);
	}

	return isCorrect;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool CheckDBMode::OnError(const DBChunk* pChunk, unsigned level, const char* pMsg)
{
	EventManager::PublishEvent(util::Format("#12Check L%u failed on file %s: %s", level,
		util::ToAnsi(pChunk->GetFilePath()).c_str(), pMsg ? pMsg : "Unknown error"));
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::OnError(const DBChunk* pChunk, unsigned level, const std::string& msg)
{
	return OnError(pChunk, level, msg.c_str());
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckLevel2(DBChunk* pChunk)
{
	// Level 2: базовый уровень проверки корректности данных
	constexpr unsigned level = 2;

	const FixNumber& first = pChunk->GetFirst();
	const Number& last = pChunk->GetLast();

	// Начало и конец интервала должны быть в одном диапазоне чисел (кроме файла для чисел 1-999)
	if (first.GetLength() != last.GetLength() && (first.GetLength() > 2 || last != Number(999u)))
		return OnError(pChunk, level, "Incorrect Header (1)");
	// Кол-во чисел Лишрел не может быть больше общего кол-ва проверенных чисел
	if (pChunk->GetPrimaryLychrelC() > pChunk->GetIterationC())
		return OnError(pChunk, level, "Incorrect Header (2)");

	StepHelper steps(1);
	// Глубина поиска не должна быть ниже требуемой для этого диапазона. Значение может быть
	// равно 0, но только если в интервале файла не содержится ни одного числа Лишрел
	if (pChunk->GetSearchDepth() < steps.GetSearchLimit(last.GetLength()) &&
		(pChunk->GetSearchDepth() || pChunk->GetPrimaryLychrelC()))
		return OnError(pChunk, level, "Incorrect Header (3)");

	// Проверка блока статистики
	unsigned lowestStep = pChunk->GetMinSavedStep();
	unsigned highestStep = pChunk->GetHighestStep();
	const unsigned* numCountA = pChunk->GetNumCountA();

	uint64_t totalNumC = 0;
	unsigned lowest = 0, highest = 0;
	for (unsigned step = 1; step <= Const::MAX_STEP; ++step)
	{
		if (numCountA[step])
		{
			totalNumC += numCountA[step];
			lowest = lowest ? lowest : step;
			highest = (step > highest) ? step : highest;
		}
	}
	// NB: поле заголовка MINSTEP (функция GetMinSavedStep) появилось в 4-й версии формата.
	// Для более ранних версий функция вернёт 0, поэтому используем значение из статистики
	if (!lowestStep && pChunk->GetFormatVer() < 4)
		lowestStep = lowest;
	// NB: поле заголовка SPAL (функция GetSavedPalindromeC) появилось в 5-й
	// версии формата БД. В более ранних версиях это значение будет равно 0
	if (totalNumC != pChunk->GetSavedPalindromeC() && pChunk->GetFormatVer() >= 5)
		return OnError(pChunk, level, "Inconsistence Header<->StatBlock (1)");
	if (lowestStep > lowest || highestStep != highest)
		return OnError(pChunk, level, "Inconsistence Header<->StatBlock (2)");
	if (totalNumC + pChunk->GetPrimaryLychrelC() > pChunk->GetIterationC())
		return OnError(pChunk, level, "Inconsistence Header<->StatBlock (3)");

	// Сверяем статистику с блоком данных
	unsigned counterA[Const::MAX_STEP + 1];
	AML_FILLA(counterA, 0, Const::MAX_STEP + 1);

	for (const auto& item : pChunk->GetNumbers())
	{
		if (!item.step || item.step > Const::MAX_STEP)
			return OnError(pChunk, level, "Incorrect step value in DataBlock");
		if (item.step < lowestStep || item.step > highestStep)
			return OnError(pChunk, level, "Inconsistence Header<->DataBlock (1)");
		if (item.num < first || item.num > last)
			return OnError(pChunk, level, "Inconsistence Header<->DataBlock (2)");
		++counterA[item.step];
	}
	for (unsigned step = 0; step <= Const::MAX_STEP; ++step)
	{
		if (numCountA[step] != counterA[step])
			return OnError(pChunk, level, "Inconsistence StatBlock<->DataBlock");
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckLevel3(DBChunk* pChunk)
{
	// Level 3: проверка палиндромов блока данных
	constexpr unsigned level = 3;

	BigNumber num;
	const auto& numbers = pChunk->GetNumbers();

	if (pChunk->GetFormatVer() <= 3)
	{
		pChunk->SortNumbers();
		const size_t numberC = numbers.size();
		for (size_t i = 1; i < numberC; ++i)
		{
			const auto& item = numbers[i];
			if (numbers[i - 1].num >= item.num)
			{
				num = item.num;
				return OnError(pChunk, level, util::Format("Duplicated palindrome detected: [%u] %s",
					item.step, SeparateWithCommas(num).c_str()));
			}
		}
	}

	Number allSavedPalC;
	for (const auto& item : numbers)
	{
		num = item.num;
		unsigned doneC = 0;
		allSavedPalC += 1 + num.GetKinNumberC();
		if (!num.RAATillPalindrome(item.step, doneC) || doneC != item.step)
		{
			num = item.num;
			return OnError(pChunk, level, util::Format("Incorrect palindrome detected: [%u] %s",
				item.step, SeparateWithCommas(num).c_str()));
		}
	}
	// NB: поле заголовка ASPAL (функция GetAllSavedPalindromeC) появилось в
	// 5-й версии формата. В более ранних версиях это значение будет равно 0
	if (pChunk->GetAllSavedPalindromeC() != allSavedPalC && pChunk->GetFormatVer() >= 5)
		return OnError(pChunk, level, "Incorrect Header:ASPAL value");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckLevel4(DBChunk* pChunk)
{
	// Level 4: проверка корректности значений в заголовке: количества
	// проверенных чисел и последнего проверенного числа в файле
	constexpr unsigned level = 4;

	BigNumber num;
	uint64_t iterationC = 0;
	const Number& last = pChunk->GetLast();

	for (num = pChunk->GetFirst();; ++num)
	{
		++iterationC;
		num.SkipRAADups();

		if (num >= last)
		{
			if (num == last)
				break;
			return OnError(pChunk, level, "Incorrect Header:LAST value");
		}

		if (!(iterationC & 0x7ffff) && util::SystemConsole::Instance().IsCtrlCPressed())
		{
			// NB: в случае отмены операции мы должны вернуть true, так как ошибок не
			// обнаружено. Проверяемый файл при этом не будет отмечен как проверенный
			m_IsCancelled = true;
			return true;
		}
	}
	if (pChunk->GetIterationC() != iterationC)
		return OnError(pChunk, level, "Incorrect Header:ITER value");

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool CheckDBMode::CheckLevel5Or6(DBChunk* pChunk, unsigned level)
{
	// Level 5: глубокий анализ для проверки количества первичных чисел Лишрел, а также полного их
	// количества. Level 6: самый глубокий анализ для проверки ошибочно пропущенных палиндромов
	if (level < 5 || level > 6)
		return false;

	StepHelper steps(1);
	const Number& last = pChunk->GetLast();
	unsigned lowestStep = pChunk->GetMinSavedStep();
	// NB: значение минимального сохраняемого шага появилось только в 4-й версии формата БД. Поэтому,
	// если это файл 3-й версии, то значение будет равно 0. Тогда попытаемся взять его из статистики
	if (!lowestStep)
	{
		const unsigned* numCountA = pChunk->GetNumCountA();
		for (unsigned step = 1; step <= Const::MAX_STEP; ++step)
		{
			if (numCountA[step])
			{
				if (!lowestStep || step < lowestStep)
					lowestStep = step;
				break;
			}
		}
		// Но если в файле вообще нет сохранённых палиндромов, то единственное,
		// что остаётся - это проверять все его числа на полную глубину
		if (!lowestStep)
			lowestStep = steps.GetSearchLimit(last) + 1;
	}

	size_t conseqLen = 20;
	if (last.GetLength() + 4 > conseqLen)
		conseqLen = std::min(last.GetLength() + 4, Const::MAX_DIGIT_C);

	BigNumber num, cur;
	uint64_t iterationC = 0;
	uint64_t palindromeC = 0;
	Number allLychrelC, cnum;

	pChunk->SortNumbers();
	const auto& numbers = pChunk->GetNumbers();
	auto nextPalindrome = numbers.cbegin();

	for (num = pChunk->GetFirst();; ++num)
	{
		++iterationC;
		num.SkipRAADups();

		cur = num;
		unsigned totalDoneC = 0, doneC = 0;
		// Случай 1: num - несохранённый палиндром с шагом ниже наименьшего сохраняемого для чисел этой длины.
		// Но сначала выполним conseqLen операций RAA, чтобы попытаться отсеять число, если это число Лишрел
		if (cur.RAATillLength(conseqLen, totalDoneC))
		{
			++palindromeC;
			if (totalDoneC >= lowestStep)
			{
				if (nextPalindrome == numbers.end() || num != nextPalindrome->num)
				{
					return OnError(pChunk, level, util::Format("Missing palindrome detected: [%u] %s",
						totalDoneC, SeparateWithCommas(num).c_str()));
				}
				++nextPalindrome;
			}
		}
		// Случай 2: число не стало палиндромом, попытаемся его отсеять,
		// то есть проверим, не является ли оно числом Лишрел
		else if (m_LychThreads.Exists(cur))
			allLychrelC += 1 + num.GetKinNumberC();
		else
		{
			// Случай 3: num - найденный палиндром из блока данных
			if (nextPalindrome != numbers.end() && num == nextPalindrome->num)
			{
				++palindromeC;
				++nextPalindrome;
			} else
			{
				cnum = cur;
				// Так как число не отсеялось, не стало палиндромом и не нашлось в блоке данных, снова проверим его
				// на палиндром (случай 1). Для этого достаточно глубины на 1 меньше минимально сохраняемого шага
				if (lowestStep > totalDoneC + 1 && cur.RAATillPalindrome(lowestStep - totalDoneC - 1, doneC))
					++palindromeC;
				else
				{
					// Случай 4: num - это число Лишрел. Тут мы должны проверить, что num действительно
					// не становится палиндромом за steps.GetSearchLimit(num.GetLength()) шагов, чтобы
					// убедиться, что num не является пропущенным отложенным палиндромом
					if (level >= 6)
					{
						totalDoneC += doneC;
						const unsigned stepLimit = steps.GetSearchLimit(num.GetLength());
						if (totalDoneC < stepLimit && cur.RAATillPalindrome(stepLimit - totalDoneC, doneC))
						{
							return OnError(pChunk, level, util::Format("Missing palindrome detected: [%u] %s",
								totalDoneC + doneC, SeparateWithCommas(num).c_str()));
						}
					}
					allLychrelC += 1 + num.GetKinNumberC();
					m_LychThreads.Insert(cnum);
				}
			}
		}

		if (num == last)
			break;

		if (!(iterationC & 0x3fff) && util::SystemConsole::Instance().IsCtrlCPressed())
		{
			// NB: в случае отмены операции мы должны вернуть true, так как ошибок не
			// обнаружено. Проверяемый файл при этом не будет отмечен как проверенный
			m_IsCancelled = true;
			return true;
		}
	}

	if (pChunk->GetIterationC() != iterationC)
		return OnError(pChunk, level, "Incorrect Header:ITER value");
	if (pChunk->GetPrimaryLychrelC() != iterationC - palindromeC)
		return OnError(pChunk, level, "Incorrect Header:LYCH value");
	if (pChunk->GetAllLychrelC() != allLychrelC && pChunk->GetFormatVer() > 3)
		return OnError(pChunk, level, "Incorrect Header:ALYCH value");

	if (nextPalindrome != numbers.end())
	{
		return OnError(pChunk, level, util::Format("Incorrect palindrome found in DataBlock: [%u] %s",
			nextPalindrome->step, SeparateWithCommas(nextPalindrome->num).c_str()));
	}

	return true;
}
