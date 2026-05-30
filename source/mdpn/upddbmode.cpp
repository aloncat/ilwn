//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "upddbmode.h"

#include "dbase.h"
#include "dbprogress.h"
#include "eventmgr.h"
#include "log.h"
#include "stephlp.h"
#include "ttime.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/console.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

//--------------------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::Progress::ResetLastTick()
{
	lastTick = ::GetTickCount();
}

//--------------------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::Progress::GetTotalElapsed() const
{
	uint32_t now = ::GetTickCount();
	return totalSeconds + .001f * (now - startTime);
}

//--------------------------------------------------------------------------------------------------------------------------------
UpdateDBMode::UpdateDBMode()
{
	AML_FILLA(m_RangeProgress, 0, util::CountOf(m_RangeProgress));
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::Run()
{
	if (!m_IsExecuted)
	{
		// Команда "update" - обновление существующих файлов БД и проверка пропущенных интервалов чисел.
		// Опционально может быть указан один из параметров: "--skipgaps", "--fromknown" или "--compress"
		if (m_Params.size() >= 1 && m_Params.size() <= 2 && !util::StrInsCmp(m_Params[0], "update"))
		{
			bool okToGo = true;
			if (m_Params.size() == 2)
			{
				// Параметр "--skipgaps" отключает проверку всех
				// непроверенных (пропущенных) интервалов чисел
				if (!util::StrInsCmp(m_Params[1], "--skipgaps"))
					m_DontFillGaps = true;
				// Параметр "--fromknown" отключает только проверку пропущенного
				// интервала перед первым существующим файлом базы данных
				else if (!util::StrInsCmp(m_Params[1], "--fromknown"))
					m_From1stKnown = true;
				// Параметр "--compress" заставляет пересохранить все
				// файлы БД с максимально возможной степенью сжатия
				else if (!util::StrInsCmp(m_Params[1], "--compress"))
					m_MaxCompression = true;
				else
					okToGo = false;
			}

			if (okToGo)
			{
				m_IsExecuted = true;
				return UpdateDataBase();
			}
		}

		OnInvalidCmdLine();
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::UpdateDataBase()
{
	m_Progress.startTime = ::GetTickCount();
	// Так как база данных может содержать очень большое количество файлов,
	// то загружаем её в состоянии HEADERONLY с целью экономии памяти
	if (!m_Data.Init(false, DBChunkState::HEADERONLY))
	{
		if (!CheckIfCancelled())
		{
			aux::Print("Failed to load database. Exiting...\n");
		}
		return false;
	}

	SystemLog::SetPath(m_Data.GetBasePath() + L"log.txt");
	PrintDataBasePath(m_Data.GetBasePath(), 46);

	EventManager::PublishEvent("Database loaded. Analyzing data chunks...");

	float timeInWork = 0;
	if (RemoveOverlaps())
	{
		size_t startRange = 0;
		m_Data.ForEachChunk([&](DBChunk* chunk) {
			startRange = chunk->GetFirst().GetLength();
			return false;
		});

		m_Steps = std::make_unique<StepHelper>(startRange);
		m_Events = std::make_unique<EventManager>(m_Data);

		timeInWork += UpdateAllChunks();
	}

	SystemLog::Instance().Close();
	aux::Printf("Time in work: %.1fs\n", timeInWork);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::RemoveOverlaps()
{
	Number first, last;
	DBChunk* prevChunk = nullptr;
	std::vector<DBChunk*> toRemove;
	toRemove.reserve(10000);

	m_Data.ForEachChunk([&](DBChunk* chunk) {
		if (first = chunk->GetFirst(); first <= last)
		{
			// Если у обоих файлов совпадает первое проверенное число,
			// то по возможности оставляем тот, что имеет актуальный формат
			if (prevChunk && prevChunk->GetFirst() == first &&
				!chunk->HasOldFormat() && prevChunk->HasOldFormat())
			{
				toRemove.push_back(prevChunk);
			} else
			{
				toRemove.push_back(chunk);
				return true;
			}
		}
		last = chunk->GetLast();
		prevChunk = chunk;
		return true;
	});

	if (!toRemove.empty())
	{
		DBProgress onProgress("Removing files: %.1f%%...");
		EventManager::PublishEvent("#6Detected overlapping regions. Removing files...");

		size_t removedCount = 0;
		for (DBChunk* chunk : toRemove)
		{
			if (!(removedCount++ & 0x3f))
			{
				onProgress(100.f * (removedCount - 1) / toRemove.size());
				if (CheckIfCancelled())
					return false;
			}

			if (!RemoveChunk(chunk))
				return false;
		}

		EventManager::PublishEvent(util::Format("  > Successfully removed %u file(s)", toRemove.size()));
		m_RemovedFileCount += toRemove.size();
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::UpdateAllChunks()
{
	const size_t totalChunkCount = m_Data.GetChunkC();

	std::vector<std::pair<DBChunk*, bool>> chunkList;
	chunkList.reserve(totalChunkCount);

	Number first, last;
	size_t gapsFound = 0;
	bool hasShortFiles = false;
	size_t testedCount = 0, toUpdateCount = 0;
	size_t compressedCount = 0;

	DBProgress onProgress("Parsing files: %.1f%%...");
	AML_FILLA(m_RangeProgress, 0, util::CountOf(m_RangeProgress));

	m_Data.ForEachChunk([&](DBChunk* chunk) {
		if (!(testedCount++ & 0x3f))
		{
			onProgress(100.f * (testedCount - 1) / totalChunkCount);
			if (CheckIfCancelled())
				return false;
		}

		if (!LoadChunkData(chunk, DBChunkState::WITHSTATS))
		{
			m_IsCancelled = true;
			return false;
		}

		first = chunk->GetFirst();
		if (last + 1u < first)
			++gapsFound;
		last = chunk->GetLast();

		if (chunk->GetDataSize() < Const::DATA_SAVE_SIZE / 2)
			hasShortFiles = true;

		const bool needsUpdate = NeedsUpdate(chunk);
		chunkList.emplace_back(chunk, needsUpdate);
		if (needsUpdate)
		{
			++toUpdateCount;
		} else
		{
			if (chunk->IsMaxCompressed())
				++compressedCount;
			size_t range = chunk->GetLast().GetLength();
			if (range >= 4 && range <= Const::MAX_DIGIT_C)
				m_RangeProgress[range] += chunk->GetIterationC();
		}

		chunk->UnloadData(DBChunkState::HEADERONLY);
		return true;
	});

	if (!m_IsCancelled)
	{
		m_From1stKnown |= m_MaxCompression;
		if (gapsFound == 1 && m_From1stKnown)
		{
			DBChunk* chunk = chunkList.front().first;
			last = chunk->GetFirst();
			--last;

			if (last.GetLength() < chunk->GetFirst().GetLength())
				gapsFound = 0;
		}

		if (m_MaxCompression)
		{
			return CompressChunks(chunkList);
		}

		if (toUpdateCount || (gapsFound && !m_DontFillGaps) || hasShortFiles)
		{
			EventManager::PublishEvent(!gapsFound ? "  > Database has #15no gaps" :
				m_DontFillGaps ? "#12Database has gaps, #7but they will be skipped" :
				"Database has #15gaps to be filled #7(task in queue)...");

			if (toUpdateCount)
			{
				EventManager::PublishEvent(util::Format("Update of %u database file(s) queued...", toUpdateCount));
			}
			else if (!gapsFound)
			{
				EventManager::PublishEvent(util::Format("No files to update (%u of %u are compressed)",
					compressedCount, m_Data.GetChunkC()));
			}

			return UpdateChunks(chunkList);
		}

		EventManager::PublishEvent("Database needs #15no update#7. Exiting...");
	}

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::UpdateChunks(const std::vector<std::pair<DBChunk*, bool>>& chunks)
{
	m_Progress.ResetLastTick();

	Number first, last;
	bool hasGaps = false;
	if (m_From1stKnown && !chunks.empty())
	{
		DBChunk* chunk = chunks.front().first;
		last = chunk->GetFirst();
		--last;

		if (last.GetLength() == chunk->GetFirst().GetLength() && !last.IsZero())
			hasGaps = true;
	}

	bool errorFlag = false;
	size_t chunksProcessed = 0;
	size_t removedCount = 0;

	for (auto& item : chunks)
	{
		++chunksProcessed;
		if (PrintProgress(chunksProcessed, chunks.size()))
			break;

		DBChunk* chunk = item.first;
		first = chunk->GetFirst();
		if (last + 1u < first)
		{
			if (m_DontFillGaps)
			{
				hasGaps = true;
			} else
			{
				DBChunkData::DataItems numbers;
				DoSearch(last + 1u, first - 1u, KnownInfo(numbers));

				if (m_IsCancelled)
					break;
			}
		}

		last = chunk->GetLast();

		if (item.second)
		{
			if (!LoadChunkData(chunk, DBChunkState::FULLDATA))
			{
				errorFlag = true;
				break;
			}

			chunk->SortNumbers();
			KnownInfo known(chunk->GetNumbers());
			known.minSavedStep = GetMinSavedStep(chunk);
			known.searchDepth = chunk->GetSearchDepth();
			known.CPUTimeShare = static_cast<float>(chunk->GetCPUTimeSpent());
			known.CPUTimeShare /= std::max(chunk->GetIterationC(), 1ull);

			DoSearch(first, chunk->GetLast(), known);
			last = chunk->GetLast();

			if (m_IsCancelled)
				break;

			if (!RemoveChunk(chunk))
			{
				errorFlag = true;
				break;
			}
			++removedCount;
		}
	}

	if (!m_IsCancelled && !errorFlag)
	{
		if (m_SavedFileCount || removedCount)
		{
			EventManager::PublishEvent(util::Format("Update finished. Files"
				" added: %u, removed: %u", m_SavedFileCount, removedCount));
		}

		m_RemovedFileCount += removedCount;

		if (!hasGaps)
			MergeChunks();

		EventManager::PublishEvent(util::Format("Total files removed: %u, remains: %u",
			m_RemovedFileCount, m_Data.GetChunkC()));
	}

	return m_Progress.GetTotalElapsed();
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::CompressChunks(const std::vector<std::pair<DBChunk*, bool>>& chunks)
{
	size_t chunksProcessed = 0;
	size_t chunksCompressed = 0;
	size_t chunksSkipped = 0;
	uint64_t dataSizeBefore = 0;
	uint64_t dataSizeAfter = 0;
	bool errorFlag = false;

	m_Progress.ResetLastTick();

	for (auto& item : chunks)
	{
		++chunksProcessed;
		if (PrintProgress(chunksProcessed, chunks.size()))
			break;

		DBChunk* chunk = item.first;
		if (chunk->GetFormatVer() != DBChunkData::LATEST_FORMAT_VERSION || !chunk->GetMinSavedStep())
		{
			++chunksSkipped;
			EventManager::PublishEvent(util::Format("Skipped file %s, because of old format",
				chunk->GetFilePath().c_str()));
		}
		else if (chunk->IsMaxCompressed())
		{
			++chunksSkipped;
		} else
		{
			if (!LoadChunkData(chunk, DBChunkState::FULLDATA))
			{
				errorFlag = true;
				break;
			}

			m_Data.SetActiveChunk(chunk);
			dataSizeBefore += chunk->GetFileSize();
			m_Data.Save(0u, 0, 0, true);
			dataSizeAfter += chunk->GetFileSize();

			chunk->UnloadData(DBChunkState::DATAUNLOADED);
			++chunksCompressed;
		}

		if (m_IsCancelled)
			break;
	}

	if (!m_IsCancelled && !errorFlag)
	{
		EventManager::PublishEvent(util::Format("Update finished. Files compressed: %u, skipped: %u",
			chunksCompressed, chunksSkipped));

		if (chunksCompressed)
		{
			float ratio = 100.f * (dataSizeBefore - dataSizeAfter) / std::max(dataSizeBefore, 1ull);
			EventManager::PublishEvent(util::Format("File size: %s -> %s, %.1f%% smaller",
				FormatSize(dataSizeBefore, true).c_str(),
				FormatSize(dataSizeAfter, true).c_str(),
				ratio));
		}
	}

	return m_Progress.GetTotalElapsed();
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::DoSearch(const Number& startFrom, const Number& target, KnownInfo known)
{
	Assert(m_Steps && m_Events && !m_activeChunk && startFrom <= target);
	CreateNewChunk(startFrom);

	unsigned stepLimit = m_Steps->GetSearchLimit(m_Last + 1u);
	// Если для "старого" файла известно значение шага, начиная с которого в нём сохранялись палиндромы,
	// и это значение меньше дефолтной глубины поиска, а глубина поиска в "старом" файле была не ниже
	// необходимой, то мы можем ипользовать при "пересчёте" файла поиск с глубиной на 1 меньше самого
	// меньшего найденного шага. В случае, если проверяемое число не было сохранено в "старом" файле
	// и не стало палиндромом за указанное количество шагов, то это число является числом Лишрел
	if (known.minSavedStep && known.minSavedStep < stepLimit && known.searchDepth >= stepLimit)
		stepLimit = known.minSavedStep - 1;
	// А если глубина поиска в "старом" файле была ниже
	// необходимой, то используем дефолтную глубину
	if (known.searchDepth < stepLimit)
		known.searchDepth = stepLimit;

	ThreadTime threadTime;
	PrintProgress(m_Last + 1u, true);

	BigNumber current, cnum, lastNum = m_Last;
	size_t lastNumLength = lastNum.GetLength();
	if ((lastNum + 1u).GetLength() > lastNumLength)
		++lastNumLength;

	size_t conseqLen = 20;
	if (lastNumLength + 4 > conseqLen)
		conseqLen = std::min(lastNumLength + 4, Const::MAX_DIGIT_C);

	uint64_t testedCount = 0;
	for (auto nextKnown = known.numbers.cbegin();; ++m_Progress.counter)
	{
		++lastNum;
		lastNum.SkipRAADups();

		if (lastNum > target)
		{
			m_Last = target;
			break;
		}

		if (lastNum.GetLength() > lastNumLength)
		{
			// Переход в новый диапазон возможен только при проверке пропущенных интервалов
			Assert(known.numbers.empty() && !known.minSavedStep && known.searchDepth == stepLimit);

			if (lastNumLength >= 3)
			{
				const auto last = lastNum - 1u;
				PrintProgress(last, true);
				SaveActiveChunk(last, threadTime);
				CreateNewChunk(lastNum);
				testedCount = 0;
			}
			lastNumLength = lastNum.GetLength();
			if (lastNumLength + 4 > conseqLen)
			{
				conseqLen = std::min(lastNumLength + 4, Const::MAX_DIGIT_C);
				m_LychThreads.Clear(false);
			}
			stepLimit = m_Steps->GetSearchLimit(lastNumLength);
			known.searchDepth = stepLimit;
		}

		bool isPalindrome = false;
		unsigned totalStepsDone = 0;

		current = lastNum;
		if (current.RAATillLength(conseqLen, totalStepsDone))
		{
			isPalindrome = true;
			if (nextKnown != known.numbers.end() && lastNum == nextKnown->num)
				++nextKnown;
		}
		else if (!m_LychThreads.Exists(current))
		{
			if (nextKnown != known.numbers.end() && lastNum == nextKnown->num)
			{
				isPalindrome = true;
				totalStepsDone = nextKnown->step;
				++nextKnown;
			} else
			{
				cnum = current;
				if (totalStepsDone < stepLimit)
				{
					unsigned stepsDone;
					isPalindrome = current.RAATillPalindrome(stepLimit - totalStepsDone, stepsDone);
					totalStepsDone += stepsDone;
				}
				if (!isPalindrome)
					m_LychThreads.Insert(cnum);
			}
		}

		if (!isPalindrome)
		{
			m_Data.AddLychrel(lastNum, known.searchDepth);
		}
		else if (totalStepsDone > Const::MAX_STEP)
		{
			m_Events->PublishAll();
			EventManager::PublishEvent(util::Format("#12FATAL ERROR: #7a"
				" number is found for HUGE step #10#%u#7!", totalStepsDone));
			m_Last = --lastNum;
			m_IsCancelled = true;
			break;
		}
		else if (m_Steps->IsSaveable(lastNumLength, totalStepsDone))
		{
			bool alreadyFound = m_Data.HasFound(totalStepsDone);
			m_Data.AddPalindrome(lastNum, totalStepsDone);
			if (!alreadyFound && m_Steps->IsNew(totalStepsDone))
				m_Events->OnPalindromeFound(lastNum, totalStepsDone);
		} else
		{
			m_Data.AddPalindrome(totalStepsDone);
		}

		++testedCount;
		++m_RangeProgress[lastNumLength];
		if (!(m_Progress.counter & 0x3ff))
		{
			if (PrintProgress(lastNum))
			{
				m_Last = lastNum;
				break;
			}

			if (GetDataSize(m_activeChunk) >= Const::DATA_SAVE_SIZE / 4 ||
				m_activeChunk->GetNumbers().size() >= Const::DATA_SAVE_NUM_COUNT / 4)
			{
				if (m_Events->HasEvents())
				{
					PrintProgress(lastNum, true);
				}
				m_CPUTime += static_cast<unsigned>(testedCount * known.CPUTimeShare);
				SaveActiveChunk(lastNum, threadTime);
				if (lastNum < target)
					CreateNewChunk(lastNum + 1u);
				testedCount = 0;
			}
		}
	}

	if (!m_IsCancelled)
	{
		PrintProgress(m_Last, true);
		m_CPUTime += static_cast<unsigned>(testedCount * known.CPUTimeShare);
		SaveActiveChunk(m_Last, threadTime);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::MergeChunks()
{
	EventManager::PublishEvent("Checking if database can be consolidated...");

	std::vector<DBChunk*> removeList;
	removeList.reserve(65000);

	const size_t totalCount = m_Data.GetChunkC();
	size_t mergedCount = 0;

	for (;;)
	{
		bool errorFlag = false;
		bool lastInRangeMerged = false;
		size_t fileCount = m_RemovedFileCount;

		uint32_t lastTick = 0;
		unsigned dataSize = 0;

		DBChunk* activeChunk = nullptr;

		m_Data.ForEachChunk([&](DBChunk* chunk) {
			bool isLastInRange = chunk->GetLast().GetLength() < (chunk->GetLast() + 1u).GetLength();
			bool canMerge = activeChunk && activeChunk->GetLast() + 1u == chunk->GetFirst() &&
				activeChunk->GetLast().GetLength() == chunk->GetFirst().GetLength() &&
				activeChunk->GetMinSavedStep() == chunk->GetMinSavedStep();
			if (canMerge && (dataSize + chunk->GetDataSize() < 13 * Const::DATA_SAVE_SIZE / 12 ||
				(isLastInRange && dataSize + chunk->GetDataSize() < 6 * Const::DATA_SAVE_SIZE / 5)))
			{
				if ((activeChunk->GetDataState() < DBChunkState::FULLDATA &&
					!LoadChunkData(activeChunk, DBChunkState::FULLDATA)) ||
					!LoadChunkData(chunk, DBChunkState::FULLDATA))
				{
					errorFlag = true;
					return false;
				}

				activeChunk->Append(chunk);
				// Здесь мы не знаем точно, каким получится размер данных в результирующем чанке (он вычисляется только
				// в момент сохранения чанка). Однако тестирование показало, что при простом сложении ошибка весьма
				// незначительна и всегда завышает размер (при сохранении объединённый чанк будет меньше)
				dataSize += chunk->GetDataSize();
				lastInRangeMerged = isLastInRange;
				++mergedCount;

				chunk->UnloadData(DBChunkState::DATAUNLOADED);
				removeList.push_back(chunk);
			} else
			{
				if (activeChunk)
				{
					m_Data.Save(0u, 0, 0);
					activeChunk->UnloadData(DBChunkState::HEADERONLY);
				}

				dataSize = chunk->GetDataSize();
				m_Data.SetActiveChunk(chunk);
				activeChunk = chunk;
			}

			++fileCount;
			if (auto tick = ::GetTickCount(); tick - lastTick >= 500)
			{
				lastTick = tick;
				aux::Printf("\rFiles merged/total: %u/%u of %u",
					mergedCount, fileCount, totalCount);
			}

			return !lastInRangeMerged;
		});

		if (errorFlag)
			return;

		if (activeChunk)
		{
			m_Data.Save(0u, 0, 0);
			activeChunk->UnloadData(DBChunkState::HEADERONLY);
			activeChunk = nullptr;
		}

		const std::string text(", removing files...");
		aux::Print(text);

		m_RemovedFileCount += removeList.size();
		for (DBChunk* chunk : removeList)
			m_Data.RemoveChunk(chunk);
		removeList.clear();

		aux::Print(EraseTextSequence(text.length(), true));

		if (!lastInRangeMerged)
		{
			EventManager::PublishEvent(util::Format("  > Consolidation done. Files merged/total: %u/%u",
				mergedCount, fileCount));
			break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::RemoveChunk(DBChunk* chunk)
{
	if (m_Data.RemoveChunk(chunk))
		return true;

	std::wstring filePath = chunk->GetFilePath();
	EventManager::PublishEvent(util::Format("#12Error: #7failed to remove file %s. #12Aborting...",
		util::ToAnsi(filePath).c_str()));
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::CreateNewChunk(const Number& first)
{
	Assert(!m_activeChunk && first);

	m_activeChunk = new DBChunk;
	m_activeChunk->Init(first, 125000);

	m_Data.SetActiveChunk(m_activeChunk);

	m_Last = first - 1u;
	m_CPUTime = 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::LoadChunkData(DBChunk* chunk, DBChunkState dataState)
{
	if (Verify(chunk))
	{
		if (chunk->LoadData(m_Data, dataState))
			return true;

		EventManager::PublishEvent(util::Format("#12Error: #7failed to load DB chunk %s. #12Aborting...",
			util::ToAnsi(chunk->GetFilePath()).c_str()));
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void UpdateDBMode::SaveActiveChunk(const Number& last, ThreadTime& threadTime)
{
	m_Last = last;
	m_CPUTime += static_cast<unsigned>(threadTime.GetElapsed() / 1000);
	threadTime.Reset();

	SaveActiveChunk();
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::SaveActiveChunk()
{
	if (m_activeChunk)
	{
		Number first;
		first = m_activeChunk->GetFirst();
		Assert(m_Steps && first && first <= m_Last);

		m_Data.Save(m_Last, m_Steps->GetMinSaveable(m_Last), m_CPUTime);
		++m_SavedFileCount;

		const size_t numberCount = m_activeChunk->GetNumbers().size();
		const size_t dataSize = GetDataSize(m_activeChunk, m_activeChunk->GetDataSize());
		EventManager::PublishEvent(util::Format("Results saved [%u/%.0fKiB]",
			numberCount, (1.f / 1024) * dataSize), true);

		m_activeChunk->UnloadData(DBChunkState::HEADERONLY);
		m_Last.SetZero();
		m_CPUTime = 0;

		m_activeChunk = nullptr;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::NeedsUpdate(const DBChunk* chunk) const
{
	Assert(m_Steps && chunk);

	const size_t digitC = chunk->GetFirst().GetLength();
	// В самых первых версиях формата v5 (используемого сейчас) не сохранялось поле MINSTEP (я решил
	// не менять версию формата, при загрузке БД этот параметр вычисляется, а здесь я добавил проверку),
	// поэтому если это поле отсутствует в чанке, то его необходимо обновить
	static_assert(DBChunkData::LATEST_FORMAT_VERSION == 5, "Remove obsolete condition below. See comment above");
	return chunk->HasOldFormat() || !chunk->GetMinSavedStep() ||
		GetMinSavedStep(chunk) > m_Steps->GetMinSaveable(digitC);
}

//----------------------------------------------------------------------------------------------------------------------
unsigned UpdateDBMode::GetMinSavedStep(const DBChunk* chunk) const
{
	Assert(chunk);

	unsigned minSavedStep = chunk->GetMinSavedStep();
	if (!minSavedStep || minSavedStep > Const::MAX_STEP)
		minSavedStep = Const::MAX_STEP;

	const unsigned* numCounters = chunk->GetNumCounters();
	for (unsigned step = 1; step <= minSavedStep; ++step)
	{
		if (numCounters[step])
			return step;
	}
	return minSavedStep;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::PrintProgress(const Number& last, bool always)
{
	const uint32_t tick = ::GetTickCount();
	if (tick - m_Progress.lastTick >= 500)
	{
		const size_t numLength = last.GetLength();
		Assert(m_Events && m_activeChunk && last && numLength <= Const::MAX_DIGIT_C);

		m_Events->PublishAll();

		const uint32_t secElapsed = (tick - m_Progress.startTime) / 1000;
		m_Progress.startTime += 1000 * secElapsed;
		m_Progress.totalSeconds += secElapsed;

		const uint32_t elapsed = tick - m_Progress.lastTick;
		const float newSpeed = (elapsed > 50) ? 1000.f * m_Progress.counter / elapsed : m_Progress.lastSpeed;
		const float speed = (m_Progress.lastSpeed <= 0) ? newSpeed : .5f * (m_Progress.lastSpeed + newSpeed);

		m_Progress.counter = 0;
		m_Progress.lastTick = tick;
		m_Progress.lastSpeed = newSpeed;

		const size_t numberCount = m_activeChunk->GetNumbers().size();
		const size_t dataSize = GetDataSize(m_activeChunk);
		const float progress = GetRangeProgress(numLength, m_RangeProgress[numLength]);

		aux::Printf("#8\r[1] #15#%s#7 [%s], %u/%s, %.3f%% done...     \b\b\b\b\b",
			SeparateWithCommas(last).c_str(), FormatSpeed(speed).c_str(),
			numberCount, FormatSize(dataSize, true).c_str(), progress);

		if (util::SystemConsole::Instance().IsCtrlCPressed())
		{
			m_IsCancelled = true;
			aux::Printc("\b\b\b. #12Stopping...\n");
		}
	}

	return m_IsCancelled;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::PrintProgress(size_t doneCount, size_t total)
{
	const uint32_t tick = ::GetTickCount();
	if (tick - m_Progress.lastTick >= 500)
	{
		Assert(m_Events && total);
		m_Events->PublishAll();

		const uint32_t secElapsed = (tick - m_Progress.startTime) / 1000;
		m_Progress.startTime += 1000 * secElapsed;
		m_Progress.totalSeconds += secElapsed;
		m_Progress.lastTick = tick;

		aux::Printf("\rProcessing file %u of %u...", doneCount, total);

		if (util::SystemConsole::Instance().IsCtrlCPressed())
		{
			m_IsCancelled = true;
			aux::Printc("\b\b\b. #12Stopping...\n");
		}
	}

	return m_IsCancelled;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::CheckIfCancelled()
{
	if (!m_IsCancelled && util::SystemConsole::Instance().IsCtrlCPressed())
	{
		EventManager::PublishEvent("Process was interrupted by user");
		m_IsCancelled = true;
	}
	return m_IsCancelled;
}
