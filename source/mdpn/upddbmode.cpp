//∙MDPN
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
#include <core/exception.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

//----------------------------------------------------------------------------------------------------------------------
UpdateDBMode::UpdateDBMode()
{
	AML_FILLA(m_RangeProgressA, 0, Const::MAX_DIGIT_C + 1);
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
				// Параметр "--compress" заставляет пересохранить все файлы
				// БД с максимально возможной степенью сжатия
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
	// то загружаем её в состоянии HEADERONLY с целью сэкономить память
	if (!m_Data.Init(false, DBChunkState::HEADERONLY))
	{
		if (!CheckIfCancelled())
		{
			aux::Print("Database not found, exiting...\n");
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
		m_Data.ForEachChunk([&](DBChunk* p) {
			startRange = p->GetFirst().GetLength();
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
	DBChunk* pPrevChunk = nullptr;
	std::vector<DBChunk*> toRemove;

	m_Data.ForEachChunk([&](DBChunk* pChunk) {
		first = pChunk->GetFirst();
		if (first <= last)
		{
			// Если у обоих файлов совпадает первое проверенное число,
			// то по возможности оставляем тот, что имеет актуальный формат
			if (pPrevChunk && pPrevChunk->GetFirst() == first && !pChunk->HasOldFormat() && pPrevChunk->HasOldFormat())
				toRemove.push_back(pPrevChunk);
			else
			{
				toRemove.push_back(pChunk);
				return true;
			}
		}
		last = pChunk->GetLast();
		pPrevChunk = pChunk;
		return true;
	});

	if (!toRemove.empty())
	{
		DBProgress onProgress("Removing files: %.1f%%...");
		EventManager::PublishEvent("#6Detected overlapping regions. Removing files...");

		size_t removedC = 0;
		for (DBChunk* pChunk : toRemove)
		{
			if (!(removedC++ & 0x3f))
			{
				onProgress(100.f * (removedC - 1) / toRemove.size());
				if (CheckIfCancelled())
					return false;
			}

			if (!RemoveChunk(pChunk))
				return false;
		}

		EventManager::PublishEvent(util::Format("  > Successfully removed %u file(s)", toRemove.size()));
		m_RemovedFileC += toRemove.size();
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::UpdateAllChunks()
{
	const size_t totalChunkC = m_Data.GetChunkC();

	std::vector<std::pair<DBChunk*, bool>> chunkList;
	chunkList.reserve(totalChunkC);

	Number first, last;
	size_t gapsFound = 0;
	bool hasShortFiles = false;
	size_t testedC = 0, toUpdateC = 0;
	DBProgress onProgress("Parsing files: %.1f%%...");
	AML_FILLA(m_RangeProgressA, 0, Const::MAX_DIGIT_C + 1);

	m_Data.ForEachChunk([&](DBChunk* pChunk) {
		if (!(testedC++ & 0x3f))
		{
			onProgress(100.f * (testedC - 1) / totalChunkC);
			if (CheckIfCancelled())
				return false;
		}

		if (!LoadChunkData(pChunk, DBChunkState::WITHSTATS))
		{
			m_IsCancelled = true;
			return false;
		}

		first = pChunk->GetFirst();
		if (last + 1u < first)
			++gapsFound;
		last = pChunk->GetLast();

		if (pChunk->GetDataSize() < Const::DATA_SAVE_SIZE / 2)
			hasShortFiles = true;

		const bool needsUpdate = NeedsUpdate(pChunk);
		chunkList.emplace_back(pChunk, needsUpdate);
		if (needsUpdate)
		{
			++toUpdateC;
		} else
		{
			size_t range = pChunk->GetLast().GetLength();
			if (range >= 4 && range <= Const::MAX_DIGIT_C)
				m_RangeProgressA[range] += pChunk->GetIterationC();
		}

		pChunk->UnloadData(DBChunkState::HEADERONLY);
		return true;
	});

	m_From1stKnown |= m_MaxCompression;
	if (gapsFound == 1 && m_From1stKnown)
	{
		DBChunk* pChunk = chunkList.front().first;
		last = pChunk->GetFirst();
		--last;

		if (last.GetLength() < pChunk->GetFirst().GetLength())
			gapsFound = 0;
	}

	if (!m_IsCancelled)
	{
		if (m_MaxCompression)
		{
			if (toUpdateC)
			{
				EventManager::PublishEvent("#12Before compressing, please update database first");
				return 0;
			}
			if (gapsFound)
			{
				EventManager::PublishEvent("#12Unable to compress database while it has any gaps");
				return 0;
			}

			m_Progress.lastTick = ::GetTickCount();
			return CompressChunks(chunkList);
		}

		if (toUpdateC || (gapsFound && !m_DontFillGaps) || hasShortFiles)
		{
			EventManager::PublishEvent(!gapsFound ? "Database has #10no gaps" :
				m_DontFillGaps ? "#12Database has gaps, #7but they will be skipped" :
				"#12Database has gaps to be filled #7(task in queue)...");

			if (toUpdateC)
			{
				EventManager::PublishEvent(util::Format("Update of %u database file(s) queued...", toUpdateC));
			}
			else if (!gapsFound || !m_DontFillGaps)
			{
				EventManager::PublishEvent("No chunks to update. Checking if they can be merged...");
			}

			m_Progress.lastTick = ::GetTickCount();
			return UpdateChunks(chunkList);
		}

		EventManager::PublishEvent("Database doesn't need to be updated, exiting...");
	}

	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::CompressChunks(const std::vector<std::pair<DBChunk*, bool>>& chunks)
{
	bool errorFlag = false;
	size_t chunksProcessed = 0;
	uint64_t dataSizeBefore = 0;
	uint64_t dataSizeAfter = 0;

	for (auto& item : chunks)
	{
		++chunksProcessed;
		const uint32_t tick = ::GetTickCount();
		if (tick - m_Progress.lastTick >= 500 && PrintProgress(tick, chunksProcessed, chunks.size()))
			break;

		DBChunk* pChunk = item.first;
		if (!LoadChunkData(pChunk, DBChunkState::FULLDATA))
		{
			errorFlag = true;
			break;
		}

		m_Data.SetActiveChunk(pChunk);
		dataSizeBefore += pChunk->GetFileSize();
		m_Data.Save(0u, 0, 0, true);
		dataSizeAfter += pChunk->GetFileSize();

		pChunk->UnloadData(DBChunkState::DATAUNLOADED);

		if (m_IsCancelled)
			break;
	}

	if (!errorFlag && !m_IsCancelled)
	{
		float ratio = 100.f * (dataSizeBefore - dataSizeAfter) / std::max(dataSizeBefore, 1ull);
		EventManager::PublishEvent(util::Format("Update finished, files compressed: %u", chunksProcessed));
		EventManager::PublishEvent(util::Format("%s -> %s, %.1f%% smaller", FormatSize(dataSizeBefore, true).c_str(),
			FormatSize(dataSizeAfter, true).c_str(), ratio));
	}

	const uint32_t endTime = ::GetTickCount();
	return m_Progress.totalSeconds + .001f * (endTime - m_Progress.startTime);
}

//----------------------------------------------------------------------------------------------------------------------
float UpdateDBMode::UpdateChunks(const std::vector<std::pair<DBChunk*, bool>>& chunks)
{
	Number first, last;
	if (m_From1stKnown && !chunks.empty())
	{
		DBChunk* pChunk = chunks.front().first;
		last = pChunk->GetFirst();
		--last;

		if (last.GetLength() == pChunk->GetFirst().GetLength() && !last.IsZero())
			m_HasGaps = true;
	}

	bool errorFlag = false;
	size_t chunksProcessed = 0;

	for (auto& item : chunks)
	{
		++chunksProcessed;
		const uint32_t tick = ::GetTickCount();
		if (tick - m_Progress.lastTick >= 500 && PrintProgress(tick, chunksProcessed, chunks.size()))
			break;

		DBChunk* pChunk = item.first;
		const bool needsUpdate = item.second;
		if (needsUpdate && !LoadChunkData(pChunk, DBChunkState::FULLDATA))
		{
			errorFlag = true;
			break;
		}

		first = pChunk->GetFirst();
		if (last + 1u < first)
		{
			if (m_DontFillGaps)
			{
				SaveActiveChunk();
				m_HasGaps = true;
			} else
			{
				if (!m_pActiveChunk)
					CreateNewChunk(last + 1u);
				DBChunkData::DataItems numbers;
				DoSearch(first - 1u, KnownInfo(numbers));
				if (m_IsCancelled)
					break;
			}
		}
		if (!needsUpdate)
		{
			SaveActiveChunk();
			last = pChunk->GetLast();
			pChunk->UnloadData(DBChunkState::DATAUNLOADED);
			DBChunk* prevChunk = m_pPrevChunk;
			m_pPrevChunk = pChunk;
			if (!m_HasGaps)
				MergeChunks(prevChunk);
			continue;
		}

		if (!m_pActiveChunk)
			CreateNewChunk(first);

		pChunk->SortNumbers();
		KnownInfo known(pChunk->GetNumbers());
		known.minSavedStep = GetMinSavedStep(pChunk);
		known.searchDepth = pChunk->GetSearchDepth();
		known.CPUTimeShare = static_cast<float>(pChunk->GetCPUTimeSpent());
		known.CPUTimeShare /= std::max(pChunk->GetIterationC(), 1ull);
		DoSearch(pChunk->GetLast(), known);
		last = pChunk->GetLast();

		if (m_IsCancelled)
			break;

		if (!RemoveChunk(pChunk))
		{
			errorFlag = true;
			break;
		}
		++m_RemovedFileC;
	}

	if (!errorFlag)
	{
		// В случае отмены операции сохраняем накопленные данные,
		// только если накопилось больше половины обычного объёма данных
		if (!m_IsCancelled || (m_pActiveChunk && GetDataSize(m_pActiveChunk) > Const::DATA_SAVE_SIZE / 2))
			SaveActiveChunk();

		if (!m_IsCancelled)
		{
			EventManager::PublishEvent(util::Format("Update finished, files added: %u,"
				" files removed: %u", m_SavedFileC, m_RemovedFileC));
		}
	}

	const uint32_t endTime = ::GetTickCount();
	return m_Progress.totalSeconds + .001f * (endTime - m_Progress.startTime);
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::DoSearch(const Number& target, KnownInfo known)
{
	Assert(m_Steps && m_Events && m_pActiveChunk);

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
	PrintProgress(::GetTickCount(), m_Last + 1u);

	BigNumber current, cnum, lastNum = m_Last;
	size_t lastNumLength = lastNum.GetLength();
	if ((lastNum + 1u).GetLength() > lastNumLength)
		++lastNumLength;

	size_t conseqLen = 20;
	if (lastNumLength + 4 > conseqLen)
		conseqLen = std::min(lastNumLength + 4, Const::MAX_DIGIT_C);

	uint64_t testedC = 0;
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
				PrintProgress(::GetTickCount(), last);
				SaveActiveChunk(last, threadTime);
				CreateNewChunk(lastNum);
				m_pPrevChunk = nullptr;
				testedC = 0;
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
		unsigned totalStepDoneC = 0;

		current = lastNum;
		if (current.RAATillLength(conseqLen, totalStepDoneC))
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
				totalStepDoneC = nextKnown->step;
				++nextKnown;
			} else
			{
				cnum = current;
				if (totalStepDoneC < stepLimit)
				{
					unsigned stepDoneC;
					isPalindrome = current.RAATillPalindrome(stepLimit - totalStepDoneC, stepDoneC);
					totalStepDoneC += stepDoneC;
				}
				if (!isPalindrome)
					m_LychThreads.Insert(cnum);
			}
		}

		if (!isPalindrome)
			m_Data.AddLychrel(lastNum, known.searchDepth);
		else if (totalStepDoneC > Const::MAX_STEP)
		{
			m_Events->PublishAll();
			EventManager::PublishEvent(util::Format("#12FATAL ERROR: #7a"
				" number is found for HUGE step #10#%u#7!", totalStepDoneC));
			m_Last = --lastNum;
			m_IsCancelled = true;
			break;
		}
		else if (m_Steps->IsSaveable(lastNumLength, totalStepDoneC))
		{
			bool alreadyFound = m_Data.HasFound(totalStepDoneC);
			m_Data.AddPalindrome(lastNum, totalStepDoneC);
			if (!alreadyFound && m_Steps->IsNew(totalStepDoneC))
				m_Events->OnPalindromeFound(lastNum, totalStepDoneC);
		} else
			m_Data.AddPalindrome(totalStepDoneC);

		++testedC;
		++m_RangeProgressA[lastNumLength];
		if (!(m_Progress.counter & 0x3ff))
		{
			const uint32_t tick = ::GetTickCount();
			if (tick - m_Progress.lastTick >= 500 && PrintProgress(tick, lastNum))
			{
				m_Last = lastNum;
				break;
			}

			if (GetDataSize(m_pActiveChunk) >= Const::DATA_SAVE_SIZE / 4 ||
				m_pActiveChunk->GetNumbers().size() >= Const::DATA_SAVE_NUMC / 4)
			{
				if (m_Events->HasEvents())
					PrintProgress(tick, lastNum);
				m_CPUTime += static_cast<unsigned>(testedC * known.CPUTimeShare);
				SaveActiveChunk(lastNum, threadTime);
				CreateNewChunk(lastNum + 1u);
				testedC = 0;
			}
		}
	}

	// Независимо от причины выхода из цикла поиска результаты могут быть сохранены в
	// файл. Поэтому сохраним накопленное значение затраченного процессорного времени
	m_CPUTime += static_cast<unsigned>(testedC * known.CPUTimeShare + .001f * threadTime.GetElapsed());

	if (!m_IsCancelled)
	{
		PrintProgress(::GetTickCount(), m_Last);
		aux::Print("\n");
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::RemoveChunk(DBChunk* pChunk)
{
	if (m_Data.RemoveChunk(pChunk))
		return true;

	std::wstring filePath = pChunk->GetFilePath();
	EventManager::PublishEvent(util::Format("#12Error: #7failed to remove file %s, #12aborting...",
		util::ToAnsi(filePath).c_str()));
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::CreateNewChunk(const Number& first)
{
	Assert(!m_pActiveChunk && first);

	m_pActiveChunk = new DBChunk;
	m_pActiveChunk->Init(first, 125000);

	m_Data.SetActiveChunk(m_pActiveChunk);

	m_Last = first - 1u;
	m_CPUTime = 0;
}

//----------------------------------------------------------------------------------------------------------------------
void UpdateDBMode::MergeChunks(DBChunk* pPrev)
{
	if (!m_pPrevChunk)
		return;

	bool success = true;
	DBChunk* pLast = m_pPrevChunk;
	m_pPrevChunk = nullptr;

	if (pPrev && pPrev->LoadData(m_Data, DBChunkState::HEADERONLY))
	{
		if (pPrev->GetLast() + 1u == pLast->GetFirst() &&
			pPrev->GetLast().GetLength() == pLast->GetFirst().GetLength() &&
			pLast->LoadData(m_Data, DBChunkState::HEADERONLY) &&
			pPrev->GetMinSavedStep() == pLast->GetMinSavedStep())
		{
			const size_t totalSize = pPrev->GetDataSize() + pLast->GetDataSize();
			if (totalSize < 13 * Const::DATA_SAVE_SIZE / 12 ||
				(totalSize < 5 * Const::DATA_SAVE_SIZE / 4 &&
				pLast->GetLast().GetLength() < (pLast->GetLast() + 1u).GetLength()))
			{
				success = false;
				if (LoadChunkData(pPrev, DBChunkState::FULLDATA) &&
					LoadChunkData(pLast, DBChunkState::FULLDATA))
				{
					m_Data.SetActiveChunk(pPrev);
					pPrev->Append(pLast);
					m_Data.Save(0u, 0, 0);
					m_pPrevChunk = pPrev;

					const size_t numberC = pPrev->GetNumbers().size();
					const size_t dataSize = GetDataSize(pPrev, pPrev->GetDataSize());
					EventManager::PublishEvent(util::Format("Results combined [%u/%.0fKiB]",
						numberC, (1.f / 1024) * dataSize), true);

					success = RemoveChunk(pLast);
					++m_RemovedFileC;
					pLast = nullptr;
				}
			}
			if (pLast)
				pLast->UnloadData(DBChunkState::DATAUNLOADED);
		}
		pPrev->UnloadData(DBChunkState::DATAUNLOADED);
	}
	if (!m_pPrevChunk && success)
		m_pPrevChunk = pLast;

	if (!success)
		throw util::ERuntime("Failed to merge database files");
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::LoadChunkData(DBChunk* pChunk, DBChunkState dataState)
{
	if (Verify(pChunk))
	{
		if (pChunk->LoadData(m_Data, dataState))
			return true;

		EventManager::PublishEvent(util::Format("#12Error: #7failed to load database file %s, #12aborting...",
			util::ToAnsi(pChunk->GetFilePath()).c_str()));
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
	if (m_pActiveChunk)
	{
		Number first;
		first = m_pActiveChunk->GetFirst();
		Assert(m_Steps && first && first <= m_Last);

		m_Data.Save(m_Last, m_Steps->GetMinSaveable(m_Last), m_CPUTime);
		++m_SavedFileC;

		const size_t numberC = m_pActiveChunk->GetNumbers().size();
		const size_t dataSize = GetDataSize(m_pActiveChunk, m_pActiveChunk->GetDataSize());
		EventManager::PublishEvent(util::Format("Results saved [%u/%.0fKiB]",
			numberC, (1.f / 1024) * dataSize), true);

		m_pActiveChunk->UnloadData(DBChunkState::DATAUNLOADED);
		m_Last.SetZero();
		m_CPUTime = 0;

		DBChunk* prevChunk = m_pPrevChunk;
		m_pPrevChunk = m_pActiveChunk;
		m_pActiveChunk = nullptr;

		if (prevChunk && !m_HasGaps)
			MergeChunks(prevChunk);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::NeedsUpdate(const DBChunk* pChunk) const
{
	Assert(m_Steps && pChunk);

	const size_t digitC = pChunk->GetFirst().GetLength();
	// В самых первых версиях формата v5 (используемого сейчас) не сохранялось поле MINSTEP (я решил
	// не менять версию формата, при загрузке БД этот параметр вычисляется, а здесь я добавил проверку),
	// поэтому если это поле отсутствует в чанке, то его необходимо обновить
	static_assert(DBChunkData::LATEST_FORMAT_VERSION == 5, "Remove obsolete condition below. See comment above");
	return pChunk->HasOldFormat() || !pChunk->GetMinSavedStep() ||
		GetMinSavedStep(pChunk) > m_Steps->GetMinSaveable(digitC);
}

//----------------------------------------------------------------------------------------------------------------------
unsigned UpdateDBMode::GetMinSavedStep(const DBChunk* pChunk) const
{
	Assert(pChunk);

	unsigned minSavedStep = pChunk->GetMinSavedStep();
	if (!minSavedStep || minSavedStep > Const::MAX_STEP)
		minSavedStep = Const::MAX_STEP;

	const unsigned* numCountA = pChunk->GetNumCountA();
	for (unsigned step = 1; step <= minSavedStep; ++step)
	{
		if (numCountA[step])
			return step;
	}
	return minSavedStep;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::PrintProgress(uint32_t tick, const Number& last)
{
	const size_t numLength = last.GetLength();
	Assert(m_Events && m_pActiveChunk && last && numLength <= Const::MAX_DIGIT_C);

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

	const size_t numberC = m_pActiveChunk->GetNumbers().size();
	const size_t dataSize = GetDataSize(m_pActiveChunk);
	const float progress = GetRangeProgress(numLength, m_RangeProgressA[numLength]);

	aux::Printf("#8\r[1] #15#%s#7 [%s], %u/%s, %.3f%% done...     \b\b\b\b\b",
		SeparateWithCommas(last).c_str(), FormatSpeed(speed).c_str(),
		numberC, FormatSize(dataSize, true).c_str(), progress);

	if (util::SystemConsole::Instance().IsCtrlCPressed())
	{
		m_IsCancelled = true;
		aux::Printc("\b\b\b, #12stopping...\n");
	}
	return m_IsCancelled;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::PrintProgress(uint32_t tick, size_t doneCount, size_t total)
{
	Assert(m_Events && total);
	m_Events->PublishAll();

	const uint32_t secElapsed = (tick - m_Progress.startTime) / 1000;
	m_Progress.startTime += 1000 * secElapsed;
	m_Progress.totalSeconds += secElapsed;

	const uint32_t elapsed = tick - m_Progress.lastTick;
	m_Progress.lastTick = tick;

	aux::Printf("\rProcessing chunk %u of %u...", doneCount, total);

	if (util::SystemConsole::Instance().IsCtrlCPressed())
	{
		m_IsCancelled = true;
		aux::Printc("\b\b\b, #12stopping...\n");
	}
	return m_IsCancelled;
}

//----------------------------------------------------------------------------------------------------------------------
bool UpdateDBMode::CheckIfCancelled()
{
	if (!m_IsCancelled && util::SystemConsole::Instance().IsCtrlCPressed())
	{
		EventManager::PublishEvent("Process was interrupted by user...");
		m_IsCancelled = true;
	}
	return m_IsCancelled;
}
