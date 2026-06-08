//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "const.h"
#include "dbase.h"
#include "dbchunk.h"
#include "dbmode.h"
#include "number.h"
#include "test.h"
#include "util.h"

#include <core/array.h>
#include <core/auxutil.h>
#include <core/console.h>
#include <core/strutil.h>
#include <core/toggle.h>
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
	bool CheckOverlaps();
	bool RemovePalindromes();
	bool MergeAndCompress();

	bool LoadChunkFullData(DBChunk* chunk);
	bool SaveChunkAndUnload(DBChunk* chunk, size_t& compressedCount);

	bool PrintRemoveProgress(size_t modified, size_t processed, size_t total, bool last = false);
	bool PrintMergeProgress(size_t merged, size_t compressed, size_t total, bool last = false);
	void PrintProgress(std::string_view fmtMsg, bool last, size_t v1, size_t v2, size_t v3);

	void PrintStatistics();

private:
	DataBase m_Data;

	uint64_t m_BasePalCount[Const::MAX_STEP + 1];
	uint64_t m_TotalPalCount[Const::MAX_STEP + 1];

	unsigned m_LastTick = 0;
	bool m_IsCancelled = false;
};

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::Run()
{
	AML_FILLA(m_BasePalCount, 0, util::CountOf(m_BasePalCount));
	AML_FILLA(m_TotalPalCount, 0, util::CountOf(m_TotalPalCount));
	m_IsCancelled = false;

	if (!m_Data.Init(false, DBChunkState::HEADERONLY))
	{
		// Здесь, скорее всего, просто отсутствует база данных (то есть это
		// не ошибка). В случае ошибки загрузки будет выброшено исключение
		aux::Print("Failed to load database\n");
		return false;
	}

	auto path = DBMode::TruncateDatabasePath(m_Data.GetBasePath(), 38);
	util::SystemConsole::Instance().SetTitle(util::Format(L"{%s} - ShrinkDB", path.c_str()));
	aux::Printf(L"Database loaded: %s\n", path.c_str());

	if (CheckOverlaps())
	{
		// TODO: Сейчас сообщение предлагает перезапустить программу с командой 'update', как если бы ShrinkDB
		// был одним из режимов работы основной программы. Позже следует интегрировать этот алгоритм в основную
		// программу. Перед этим нужно добавить возможность задавать minSteps через командную строку или файл.
		// Также будет неплохо добавить вывод всех установленных значений minSteps перед началом работы
		aux::Printc("#12Detected overlapping regions in the database. #7Run with '#14update#7' to fix this\n");
	}
	else if (RemovePalindromes() && MergeAndCompress())
	{
		PrintStatistics();
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::CheckOverlaps()
{
	Number last;
	bool hasOverlaps = false;

	m_Data.ForEachChunk([&](DBChunk* chunk) {
		if (chunk->GetFirst() <= last)
		{
			hasOverlaps = true;
			return 1;
		}
		last = chunk->GetLast();
		return 0;
	});

	return hasOverlaps;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::RemovePalindromes()
{
	size_t processedCount = 0, modifiedCount = 0;
	const size_t totalCount = m_Data.GetChunkC();

	m_LastTick = 0;
	int errorCode = 0;

	m_Data.ForEachChunk(errorCode, [&](DBChunk* chunk) {
		if (PrintRemoveProgress(modifiedCount, processedCount, totalCount))
			return 1;

		if (!LoadChunkFullData(chunk))
			return -1;

		const size_t range = chunk->GetLast().GetLength();
		// Первые 8 диапазонов умещаются каждый в один файл (кроме диапазонов 1-3,
		// которые хранятся в одном). Поэтому нет смысла вырезать из них что-либо
		if (range > 8 && range <= Const::MAX_DIGIT_C && minSteps[range] > 1 &&
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

		return 0;
	});

	PrintRemoveProgress(modifiedCount, processedCount, totalCount, true);
	return !errorCode;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::MergeAndCompress()
{
	std::vector<DBChunk*> removeList;
	removeList.reserve(65000);

	size_t mergedCount = 0;
	size_t compressedCount = 0;
	size_t removedCount = 0;
	size_t totalCount;

	m_LastTick = 0;
	int errorCode = 0;

	for (;;)
	{
		bool lastInRangeMerged = false;
		DBChunk* activeChunk = nullptr;
		size_t accumulatedSize = 0;
		totalCount = removedCount;

		m_Data.ForEachChunk(errorCode, [&](DBChunk* chunk) {
			if (PrintMergeProgress(mergedCount, compressedCount, totalCount))
				return 1;

			bool isLastInRange = chunk->GetLast().GetLength() < (chunk->GetLast() + 1u).GetLength();
			bool canMerge = activeChunk && activeChunk->GetLast() + 1u == chunk->GetFirst() &&
				activeChunk->GetLast().GetLength() == chunk->GetFirst().GetLength() &&
				activeChunk->GetMinSavedStep() == chunk->GetMinSavedStep();
			if (canMerge && (accumulatedSize + chunk->GetDataSize() < 13 * DATA_SAVE_SIZE / 12 ||
				(isLastInRange && accumulatedSize + chunk->GetDataSize() < 6 * DATA_SAVE_SIZE / 5)))
			{
				if (!LoadChunkFullData(activeChunk) || !LoadChunkFullData(chunk))
					return -1;

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
				if (!SaveChunkAndUnload(activeChunk, compressedCount))
					return -1;

				activeChunk = chunk;
				m_Data.SetActiveChunk(chunk);
				accumulatedSize = chunk->GetDataSize();
			}

			++totalCount;
			// Если последний чанк диапазона был объединён с предыдущим, то мы должны
			// повторить цикл, чтобы опять попытаться объединить два последних чанка
			return lastInRangeMerged ? 1 : 0;
		});

		// Так как ошибка могла случиться только при загрузке чанка, то мы должны сохранить текущий чанк,
		// а также удалить все чанки, которые мы успели объединить. При m_IsCancelled == true аналогично
		if (!SaveChunkAndUnload(activeChunk, compressedCount))
			errorCode = -1;

		{
			util::FuncToggle tempMsg([errorCode](bool doShow) {
				if (errorCode < 0)
					return false;
				const char* text = ", removing files...";
				aux::Print(doShow ? text : EraseTextSequence(strlen(text), true));
				return true;
			});

			removedCount += removeList.size();
			for (DBChunk* chunk : removeList)
				m_Data.RemoveChunk(chunk);
			removeList.clear();
		}

		if (!lastInRangeMerged || errorCode < 0 || m_IsCancelled)
			break;
	}

	if (errorCode >= 0 && !PrintMergeProgress(mergedCount, compressedCount, totalCount, true))
	{
		aux::Printf("Files removed: %u, remains: %u\n",
			removedCount, m_Data.GetChunkC());
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::LoadChunkFullData(DBChunk* chunk)
{
	if (chunk && chunk->LoadData(m_Data, DBChunkState::FULLDATA))
		return true;

	aux::Printc("#12\rError: failed to load DB chunk\n");
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::SaveChunkAndUnload(DBChunk* chunk, size_t& compressedCount)
{
	if (chunk)
	{
		if (chunk->GetSaveState() > DBChunkState::UNCHANGED)
		{
			m_Data.Save(0u, 0, 0, true);
			++compressedCount;
		}
		else if (!chunk->IsMaxCompressed())
		{
			if (!LoadChunkFullData(chunk))
				return false;

			m_Data.Save(0u, 0, 0, true);
			++compressedCount;
		}
		chunk->UnloadData(DBChunkState::HEADERONLY);
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::PrintRemoveProgress(size_t modified, size_t processed, size_t total, bool last)
{
	PrintProgress("\rFiles resized / total: %u/%u of %u",
		last, modified, processed, total);
	return m_IsCancelled;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DBShrinker::PrintMergeProgress(size_t merged, size_t compressed, size_t total, bool last)
{
	PrintProgress("\rFiles merged / compressed / total: %u/%u/%u",
		last, merged, compressed, total);
	return m_IsCancelled;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBShrinker::PrintProgress(std::string_view fmtMsg, bool last, size_t v1, size_t v2, size_t v3)
{
	if (auto tick = ::GetTickCount(); last || tick - m_LastTick >= 500)
	{
		m_LastTick = tick;

		const size_t msgLength = fmtMsg.size();
		util::SmartArray<char, 512> buffer(msgLength + 2);
		memcpy(buffer, fmtMsg.data(), msgLength);
		buffer[msgLength] = last ? '\n' : 0;
		buffer[msgLength + 1] = 0;

		aux::Printf(buffer, v1, v2, v3);

		if (util::SystemConsole::Instance().IsCtrlCPressed())
		{
			if (last)
			{
				aux::Printc("#12Process was interrupted by user\n");
			}
			m_IsCancelled = true;
		}
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
		while (firstRange <= lastRange)
			ranges[firstRange++] = true;
		return 0;
	});

	aux::Printf("Database size: %s\n",
		FormatSize(databaseSize).c_str());

	std::string s1, s2("---\nRanges:");
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
