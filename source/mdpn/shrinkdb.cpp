//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "const.h"
#include "dbase.h"
#include "dbchunk.h"
#include "number.h"
#include "test.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Удаление палиндромов с низкими шагами из БД для уменьшения её размера
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Используем увеличенное значение вместо Const::DATA_SAVE_SIZE,
// чтобы сделать чанки сокращённой БД крупнее и уменьшить их число
constexpr size_t DATA_SAVE_SIZE = 1200 * 1024;

// Шаги, начиная с которых отложенные палиндромы будут сохраняться в БД (для каждого диапазона чисел).
// При значении 1 будут сохраняться абсолютно все найденные палиндромы. Чем ниже значение, тем больше
// размер файлов. В основной БД ограничения начинаются с 13-значных чисел: 10, 15, 35, 40, 45, 45...
const unsigned minSteps[Const::MAX_DIGIT_C + 1] = { 0,
	  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,		//  1 - 10
	  1,  14,  32,  51,  61,  80,  89, 109, 118, 137,		// 11 - 20
	150, 150, 150, 150, 150, 150, 150, 150, 150, 150 };		// 21 - 30

//--------------------------------------------------------------------------------------------------------------------------------
class DBShrinker final
{
	AML_NONCOPYABLE(DBShrinker)

public:
	DBShrinker() = default;

	bool Run();

private:
	bool RemovePalindromes();

	bool MergeAndCompress();
	bool SaveAndUnload(DBChunk* chunk);

	void PrintRemoveProgress(size_t modified, size_t processed, size_t total, bool last = false);
	void PrintMergeProgress(size_t merged, size_t compressed, size_t total, bool last = false);

	void PrintStatistics();

private:
	DataBase m_Data;

	uint64_t m_BasePalCount[Const::MAX_STEP + 1];
	uint64_t m_TotalPalCount[Const::MAX_STEP + 1];
	unsigned m_LastTick = 0;
};

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::Run()
{
	AML_FILLA(m_BasePalCount, 0, util::CountOf(m_BasePalCount));
	AML_FILLA(m_TotalPalCount, 0, util::CountOf(m_TotalPalCount));

	if (!m_Data.Init(false, DBChunkState::HEADERONLY))
	{
		aux::Print("Failed to load database\n");
		return false;
	}

	if (RemovePalindromes() && MergeAndCompress())
	{
		PrintStatistics();
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::RemovePalindromes()
{
	size_t processedCount = 0, modifiedCount = 0;
	const size_t totalCount = m_Data.GetChunkC();

	m_LastTick = 0;
	bool errorFlag = false;

	m_Data.ForEachChunk([&](DBChunk* chunk) {
		if (!chunk->LoadData(m_Data, DBChunkState::FULLDATA))
		{
			errorFlag = true;
			aux::Printc("#12\rError: failed to load DB chunk\n");
			return false;
		}

		const size_t range = chunk->GetLast().GetLength();
		if (range > 3 && range <= Const::MAX_DIGIT_C && minSteps[range] > 1 &&
			chunk->GetMinSavedStep() < minSteps[range])
		{
			if (chunk->RemovePalindromes(minSteps[range]))
			{
				m_Data.SetActiveChunk(chunk);
				m_Data.Save(0u, 0, 0);
				++modifiedCount;
			}
		}

		Number num;
		for (unsigned i = 1; i <= chunk->GetHighestStep(); ++i)
			m_BasePalCount[i] += chunk->GetNumCounters()[i];
		for (const auto& item : chunk->GetNumbers())
		{
			num = item.num;
			m_TotalPalCount[item.step] += 1 + num.GetKinNumberCount();
		}

		++processedCount;
		chunk->UnloadData(DBChunkState::HEADERONLY);
		PrintRemoveProgress(modifiedCount, processedCount, totalCount);
		return true;
	});

	PrintRemoveProgress(modifiedCount, processedCount, totalCount, true);
	return !errorFlag;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::MergeAndCompress()
{
	std::vector<DBChunk*> removeList;
	removeList.reserve(65000);

	size_t mergedCount = 0;
	size_t compressedCount = 0;
	size_t removedCount = 0;

	m_LastTick = 0;
	bool errorFlag = false;

	for (;;)
	{
		bool lastInRangeMerged = false;
		DBChunk* activeChunk = nullptr;
		size_t totalCount = removedCount;
		size_t accumulatedSize = 0;

		m_Data.ForEachChunk([&](DBChunk* chunk) {
			bool isLastInRange = chunk->GetLast().GetLength() < (chunk->GetLast() + 1u).GetLength();
			bool canMerge = activeChunk && activeChunk->GetLast() + 1u == chunk->GetFirst() &&
				activeChunk->GetLast().GetLength() == chunk->GetFirst().GetLength() &&
				activeChunk->GetMinSavedStep() == chunk->GetMinSavedStep();
			if (canMerge && (accumulatedSize + chunk->GetDataSize() < 13 * DATA_SAVE_SIZE / 12 ||
				(isLastInRange && accumulatedSize + chunk->GetDataSize() < 6 * DATA_SAVE_SIZE / 5)))
			{
				if (!activeChunk->LoadData(m_Data, DBChunkState::FULLDATA) ||
					!chunk->LoadData(m_Data, DBChunkState::FULLDATA))
				{
					errorFlag = true;
					aux::Printc("#12\rError: failed to load DB chunk\n");
					return false;
				}

				activeChunk->Append(chunk);
				// Здесь мы не знаем точно, каким получится размер данных в результирующем чанке (он вычисляется только
				// в момент сохранения чанка). Однако тестирование показало, что при простом сложении ошибка весьма
				// незначительна и всегда завышает размер (при сохранении объединённый чанк будет меньше)
				accumulatedSize += chunk->GetDataSize();
				lastInRangeMerged = isLastInRange;
				++mergedCount;

				chunk->UnloadData(DBChunkState::DATAUNLOADED);
				removeList.push_back(chunk);
			} else
			{
				if (SaveAndUnload(activeChunk))
					++compressedCount;

				activeChunk = chunk;
				m_Data.SetActiveChunk(chunk);
				accumulatedSize = chunk->GetDataSize();
			}

			++totalCount;
			PrintMergeProgress(mergedCount, compressedCount, totalCount);
			return !lastInRangeMerged;
		});

		if (errorFlag)
			return false;

		if (SaveAndUnload(activeChunk))
			++compressedCount;

		std::string text(", removing files...");
		aux::Print(text);

		removedCount += removeList.size();
		for (DBChunk* chunk : removeList)
			m_Data.RemoveChunk(chunk);
		removeList.clear();

		aux::Print(EraseTextSequence(text.size(), true));

		if (!lastInRangeMerged)
			break;
	}

	size_t totalFileCount = removedCount;
	m_Data.ForEachChunk([&](DBChunk* chunk) {
		if (!chunk->IsMaxCompressed())
		{
			if (!chunk->LoadData(m_Data, DBChunkState::FULLDATA))
			{
				errorFlag = true;
				aux::Printc("#12\rError: failed to load DB chunk\n");
				return false;
			}

			m_Data.SetActiveChunk(chunk);
			m_Data.Save(0u, 0, 0, true);
			++compressedCount;

			chunk->UnloadData(DBChunkState::HEADERONLY);
		}

		++totalFileCount;
		PrintMergeProgress(mergedCount, compressedCount, totalFileCount);
		return true;
	});

	if (!errorFlag)
	{
		PrintMergeProgress(mergedCount, compressedCount, totalFileCount, true);
		aux::Printf("Files removed: %u, remains: %u\n", removedCount, m_Data.GetChunkC());
	}

	return !errorFlag;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::SaveAndUnload(DBChunk* chunk)
{
	bool compressed = false;

	if (chunk)
	{
		if (chunk->GetSaveState() > DBChunkState::UNCHANGED)
		{
			m_Data.Save(0u, 0, 0, true);
			compressed = true;
		}
		chunk->UnloadData(DBChunkState::HEADERONLY);
	}

	return compressed;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBShrinker::PrintRemoveProgress(size_t modified, size_t processed, size_t total, bool last)
{
	if (auto tick = ::GetTickCount(); last || tick - m_LastTick >= 500)
	{
		m_LastTick = tick;
		char fmt[] = "\rFiles resized/total: %u/%u of %u\n";
		fmt[util::CountOf(fmt) - 2] = last ? '\n' : 0;
		aux::Printf(fmt, modified, processed, total);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBShrinker::PrintMergeProgress(size_t merged, size_t compressed, size_t total, bool last)
{
	if (auto tick = ::GetTickCount(); last || tick - m_LastTick >= 500)
	{
		m_LastTick = tick;
		char fmt[] = "\rFiles merged/compressed/total: %u/%u/%u\n";
		fmt[util::CountOf(fmt) - 2] = last ? '\n' : 0;
		aux::Printf(fmt, merged, compressed, total);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBShrinker::PrintStatistics()
{
	size_t lastRange = 0;
	uint64_t databaseSize = 0;
	bool ranges[Const::MAX_DIGIT_C + 1] = {};

	m_Data.ForEachChunk([&](DBChunk* chunk) {
		databaseSize += chunk->GetFileSize();
		auto firstRange = chunk->GetFirst().GetLength();
		lastRange = chunk->GetLast().GetLength();
		for (; firstRange <= lastRange; ++firstRange)
			ranges[firstRange] = true;
		return true;
	});

	aux::Printf("Database size: %s\n",
		FormatSize(databaseSize).c_str());

	std::string s1, s2 = "---\nRanges:";
	for (size_t i = 1; i <= lastRange; ++i)
	{
		if (i > 1 && i % 5 == 1)
			s2 += "  ";
		s2 += util::Format(" #%u#%u", ranges[i] ? 15 : 4, i);
	}
	aux::Printc(s2 + "\n---\n");

	constexpr int COLUMNS = 6;
	constexpr int STEPS_IN_COLUMN = 25;

	uint64_t lastCounts[COLUMNS] = { size_t(-1) };
	for (int i = 1; i < COLUMNS; ++i)
		lastCounts[i] = m_BasePalCount[i * STEPS_IN_COLUMN];
	for (int step = 1; step <= STEPS_IN_COLUMN; ++step)
	{
		s1.clear(); s2.clear();
		for (int i = 0; i < COLUMNS; ++i)
		{
			uint64_t total = m_BasePalCount[step + i * STEPS_IN_COLUMN];
			bool isColored = (2 * lastCounts[i] < total) && (i || step > 2);
			lastCounts[i] = total;

			s1 += i ? "   #8|#7   " : "";
			s1 += util::Format("Step %-10s #%u#%15s", util::Format("#%02u#%u#7:",
				isColored ? 14 : 11, step + i * STEPS_IN_COLUMN).c_str(),
				isColored ? 14 : 7, SeparateWithCommas(total).c_str());

			s2 += i ? "   #8|#7   " : "";
			total = m_TotalPalCount[step + i * STEPS_IN_COLUMN];
			s2 += util::Format("#%u#%25s", isColored ? 6 : 8,
				SeparateWithCommas(total).c_str());
		}
		aux::Printc(s1 + "\n" + s2 + "\n");
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
int ShrinkDBMain()
{
	if (!TestFacility::CheckRequirements(false))
	{
		aux::Printc("#12Error: #7failed to pass one or more crucial checks\n");
		return 1;
	}

	DBShrinker shrinker;
	return shrinker.Run() ? 0 : 1;
}
