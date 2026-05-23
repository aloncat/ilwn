//∙MDPN (Shrink)
#include "pch.h"

#include "const.h"
#include "dbase.h"
#include "dbchunk.h"
#include "eventmgr.h"
#include "number.h"
#include "test.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ShrinkDB - удаление палиндромов с низкими шагами из БД для уменьшения её размера
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Используем увеличенное значение вместо Const::DATA_SAVE_SIZE,
// чтобы уменьшить количество чанков в "сжатой" БД
constexpr size_t DATA_SAVE_SIZE = 1200 * 1024;

// Шаги, начиная с которых отложенные палиндромы будут сохраняться в БД (для каждого диапазона чисел).
// При значении 1 будут сохрнаяться абсолютно все найденные палиндромы. Чем ниже значение, тем больше
// размер файлов. В основной БД ограничения начинаются с 13-значных чисел: 10, 15, 35, 40, 40, 45...
const unsigned minSteps[Const::MAX_DIGIT_C + 1] = { 0,
	  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,	//  1 - 10
	  1,  14,  32,  51,  61,  80,  89, 109, 118, 120,	// 11 - 20
	115, 120, 120, 120, 120, 120, 120, 120, 120, 120 };	// 21 - 30

//--------------------------------------------------------------------------------------------------------------------------------
class Shrinker
{
	AML_NONCOPYABLE(Shrinker)

public:
	Shrinker() = default;

	bool Run();

protected:
	bool RemovePalindromes(std::set<DBChunk*>& compressList);
	void ShowStatistics();

protected:
	DataBase m_Data;

	uint64_t m_BasePalCount[Const::MAX_STEP + 1];
	uint64_t m_TotalPalCount[Const::MAX_STEP + 1];
};

//--------------------------------------------------------------------------------------------------------------------------------
bool Shrinker::Run()
{
	AML_FILLA(m_BasePalCount, 0, Const::MAX_STEP + 1);
	AML_FILLA(m_TotalPalCount, 0, Const::MAX_STEP + 1);

	if (!m_Data.Init(false, DBChunkState::HEADERONLY))
	{
		aux::Print("Failed to load database\n");
		return true;
	}

	const size_t totalFileCount = m_Data.GetChunkC();
	std::set<DBChunk*> compressList, removeList;

	RemovePalindromes(compressList);

	size_t fileCount;
	size_t mergedCount = 0;
	size_t compressedCount = 0;
	size_t removedCount = 0;

	for (;;)
	{
		bool errorFlag = false;
		bool lastInRangeMerged = false;
		fileCount = removedCount;

		uint32_t lastTick = 0;
		unsigned dataSize = 0;
		DBChunk* pActiveChunk = nullptr;

		m_Data.ForEachChunk([&](DBChunk* pChunk) {
			bool isLastInRange = pChunk->GetLast().GetLength() < (pChunk->GetLast() + 1u).GetLength();
			bool canMerge = pActiveChunk && pActiveChunk->GetLast() + 1u == pChunk->GetFirst() &&
				pActiveChunk->GetLast().GetLength() == pChunk->GetFirst().GetLength() &&
				pActiveChunk->GetMinSavedStep() == pChunk->GetMinSavedStep();
			if (canMerge && (dataSize + pChunk->GetDataSize() < 13 * DATA_SAVE_SIZE / 12 ||
				(isLastInRange && dataSize + pChunk->GetDataSize() < 6 * DATA_SAVE_SIZE / 5)))
			{
				if ((pActiveChunk->GetDataState() < DBChunkState::FULLDATA &&
					!pActiveChunk->LoadData(m_Data, DBChunkState::FULLDATA)) ||
					!pChunk->LoadData(m_Data, DBChunkState::FULLDATA))
				{
					errorFlag = true;
					aux::Printc("#12\rError: failed to load DB chunk");
					return false;
				}

				pActiveChunk->Append(pChunk);
				compressList.insert(pActiveChunk);
				// Здесь мы не знаем точно, каким получится размер данных в результирующем чанке (он вычисляется только
				// в момент сохранения чанка). Однако тестирование показало, что при простом сложении ошибка весьма
				// незначительна и всегда завышает размер (при сохранении объединённый чанк будет меньше)
				dataSize += pChunk->GetDataSize();
				lastInRangeMerged = isLastInRange;
				++mergedCount;

				pChunk->UnloadData(DBChunkState::DATAUNLOADED);
				compressList.erase(pChunk);
				removeList.insert(pChunk);
			} else
			{
				if (pActiveChunk)
				{
					if (auto it = compressList.find(pActiveChunk); it != compressList.end())
					{
						m_Data.Save(0u, 0, 0, true);
						compressList.erase(it);
						++compressedCount;
					}
					pActiveChunk->UnloadData(DBChunkState::HEADERONLY);
				}

				m_Data.SetActiveChunk(pChunk);
				dataSize = pChunk->GetDataSize();
				pActiveChunk = pChunk;
			}

			++fileCount;
			if (auto tick = ::GetTickCount(); tick - lastTick >= 500)
			{
				lastTick = tick;
				aux::Printf("\rFiles merged/compressed/total: %u/%u/%u",
					mergedCount, compressedCount, fileCount);
			}

			return !lastInRangeMerged;
		});

		if (errorFlag)
			return false;

		if (pActiveChunk)
		{
			if (auto it = compressList.find(pActiveChunk); it != compressList.end())
			{
				m_Data.Save(0u, 0, 0, true);
				compressList.erase(it);
				++compressedCount;
			}
			pActiveChunk->UnloadData(DBChunkState::HEADERONLY);
			pActiveChunk = nullptr;
		}

		std::string text(", removing files...");
		aux::Print(text);

		for (DBChunk* pChunk : removeList)
		{
			++removedCount;
			m_Data.RemoveChunk(pChunk);
		}
		aux::Print(EraseTextSequence(text.length()).c_str());

		if (removeList.empty() || !lastInRangeMerged)
		{
			aux::Printf("\rFiles merged/compressed/total: %u/%u/%u\n",
				mergedCount, compressedCount, fileCount);
			aux::Printf("Files removed: %u, remains: %u\n",
				removedCount, m_Data.GetChunkC());
			break;
		}

		removeList.clear();
	}

	ShowStatistics();
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Shrinker::RemovePalindromes(std::set<DBChunk*>& compressList)
{
	const size_t totalFileCount = m_Data.GetChunkC();

	uint32_t lastTick = 0;
	size_t fileCount = 0, modifiedCount = 0;

	m_Data.ForEachChunk([&](DBChunk* pChunk) {
		if (!pChunk->LoadData(m_Data, DBChunkState::FULLDATA))
		{
			fileCount = 0;
			aux::Printc("#12\rError: failed to load DB chunk");
			return false;
		}

		const size_t range = pChunk->GetLast().GetLength();
		if (range > 3 && range <= Const::MAX_DIGIT_C && minSteps[range] > 1 &&
			pChunk->GetMinSavedStep() < minSteps[range])
		{
			compressList.insert(pChunk);
			if (pChunk->RemovePalindromes(minSteps[range]))
			{
				m_Data.SetActiveChunk(pChunk);
				m_Data.Save(0u, 0, 0);
				++modifiedCount;
			}
		}

		Number num;
		for (unsigned i = 1; i <= pChunk->GetHighestStep(); ++i)
			m_BasePalCount[i] += pChunk->GetNumCountA()[i];
		for (const auto& item : pChunk->GetNumbers())
		{
			num = item.num;
			m_TotalPalCount[item.step] += 1 + num.GetKinNumberCount();
		}

		++fileCount;
		pChunk->UnloadData(DBChunkState::HEADERONLY);

		if (auto tick = ::GetTickCount(); tick - lastTick >= 500)
		{
			lastTick = tick;
			aux::Printf("\rFiles resized/total: %u/%u of %u",
				modifiedCount, fileCount, totalFileCount);
		}

		return true;
	});

	aux::Printf("\rFiles resized/total: %u/%u of %u\n",
		modifiedCount, fileCount, totalFileCount);

	return fileCount || !totalFileCount;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Shrinker::ShowStatistics()
{
	size_t lastRange = 0;
	uint64_t databaseSize = 0;
	bool ranges[Const::MAX_DIGIT_C + 1] = {};
	m_Data.ForEachChunk([&](DBChunk* pChunk) {
		databaseSize += pChunk->GetFileSize();
		auto firstRange = pChunk->GetFirst().GetLength();
		lastRange = pChunk->GetLast().GetLength();
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
	constexpr unsigned STEPS_IN_COLUMN = 25;
	uint64_t lastCounts[COLUMNS] = { size_t(-1) };
	for (int i = 1; i < COLUMNS; ++i)
		lastCounts[i] = m_BasePalCount[i * STEPS_IN_COLUMN];
	for (unsigned step = 1; step <= STEPS_IN_COLUMN; ++step)
	{
		s1.clear(); s2.clear();
		for (int i = 0; i < COLUMNS; ++i)
		{
			s1 += i ? "   #8|#7   " : "";
			const size_t total = m_BasePalCount[step + i * STEPS_IN_COLUMN];
			bool isColored = (2 * lastCounts[i] < total) && (i || step > 2);
			s1 += util::Format("Step %-10s #%u#%15s", util::Format("#%02u#%u#7:",
				isColored ? 14 : 11, step + i * STEPS_IN_COLUMN).c_str(),
				isColored ? 14 : 7, SeparateWithCommas(total).c_str());
			s2 += i ? "   #8|#7   " : "";
			s2 += util::Format("#%u#%25s", isColored ? 6 : 8,
				SeparateWithCommas(m_TotalPalCount[step + i * STEPS_IN_COLUMN]).c_str());
			lastCounts[i] = total;
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

	Shrinker shrinker;
	return shrinker.Run() ? 0 : 1;
}
