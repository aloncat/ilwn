//⬪MDPN⬪
#include "pch.h"

#include "chkdbmode.h"
#include "const.h"
#include "dbase.h"
#include "dbchunk.h"
#include "dbmode.h"
#include "eventmgr.h"
#include "largemempages.h"
#include "log.h"
#include "mode.h"
#include "number.h"
#include "searchmode.h"
#include "test.h"
#include "upddbmode.h"
#include "util.h"
#include "version.h"

#include <core/auxutil.h>
#include <core/console.h>
#include <core/datetime.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

#include <algorithm>
#include <functional>
#include <string.h>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   AnalyseDataBase - анализ базы данных и сбор статистики
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct RangeInfo
{
	size_t digitC = 0;				// Диапазон (длина последнего проверенного числа)

	size_t fileC = 0;				// Количество файлов БД, содержащих данные этого диапазон
	uint64_t totalFileSize = 0;		// Суммарный размер этих файлов (по данным заголовков)
	uint64_t totalDataSize = 0;		// Суммарный размер несжатых блоков данных
	uint64_t totalCDataSize = 0;	// Суммарный размер сжатых блоков данных

	uint64_t CPUTimeSpent = 0;		// Суммарное затраченное время CPU (в ms)

	uint64_t iterationC = 0;		// Общее количество проверенных чисел
	uint64_t primLychrelC = 0;		// Количество найденных первичных чисел Лишрел
	uint64_t savedNumberC = 0;		// Количество сохранённых в файлах БД палиндромов

	//Number allPalindromeC;		// Полное количество палиндрмов в базе
	Number allLychrelC;				// Полное количество чисел Лишрел в диапазоне

	unsigned lowestStep = 0;		// Наименьший шаг среди сохранённых палиндромов
	unsigned highestStep = 0;		// Наибольший шаг среди сохранённых палиндромов
	unsigned minSavedStep = 0;		// Наименьшее (среди всех файлов) значение поля MINSTEP
	unsigned searchDepth = 0;		// Наименьшее (среди всех файлов) значение поля DEPTH

	Number last;

	bool isIncomplete = false;		// true, если диапазон проверен не полностью
	bool hasOldFormat = false;		// true, если не все файлы имеют последнюю версию формата
};

//----------------------------------------------------------------------------------------------------------------------
struct PalindromeInfo
{
	Number lowestNumber;	// Число - наименьший отложенный палиндром для шага N
	Number highestNumber;	// Наибольший найденный отложенный палиндром для шага N
	Number palindrome;		// Палиндром (результат N операций RAA над lowestNumber)
	uint64_t count = 0;		// Суммарное кол-во найденных палиндромов для этого шага
};

//----------------------------------------------------------------------------------------------------------------------
static std::string FormatCPUTime(uint64_t CPUTime)
{
	uint64_t totalSeconds = (CPUTime + 500) / 1000;
	unsigned dayC = static_cast<unsigned>(totalSeconds / (24 * 3600));
	totalSeconds -= (24 * 3600) * dayC;
	unsigned hourC = static_cast<unsigned>(totalSeconds / 3600);
	totalSeconds -= 3600 * hourC;
	unsigned minuteC = static_cast<unsigned>(totalSeconds / 60);
	unsigned secondC = totalSeconds % 60;

	return dayC ? util::Format("%ud:%02u:%02u:%02u", dayC, hourC, minuteC, secondC) :
		util::Format("%02u:%02u:%02u", hourC, minuteC, secondC);
}

//----------------------------------------------------------------------------------------------------------------------
static void OutputAnalysisResults(DataBase& data, const RangeInfo* rangeA, const PalindromeInfo* palindromeA)
{
	Number lastNum;
	RangeInfo common;
	bool hasOldFormat = false;
	for (size_t i = 1; i < Const::MAX_DIGIT_C; ++i)
	{
		if (rangeA[i].fileC)
		{
			common.fileC += rangeA[i].fileC;
			common.totalFileSize += rangeA[i].totalFileSize;
			common.totalDataSize += rangeA[i].totalDataSize;
			common.totalCDataSize += rangeA[i].totalCDataSize;
			common.CPUTimeSpent += rangeA[i].CPUTimeSpent;
			common.iterationC += rangeA[i].iterationC;
			common.savedNumberC += rangeA[i].savedNumberC;
			//common.allPalindromeC += rangeA[i].allPalindromeC;
			common.highestStep = std::max(common.highestStep, rangeA[i].highestStep);
			if (lastNum < rangeA[i].last)
				lastNum = rangeA[i].last;
			if (rangeA[i].hasOldFormat)
				hasOldFormat = true;
		}
	}
////
	util::DateTime dt;
	std::string report;
////
	dt.Update();
	report += util::Format("Database analysis report, generated on %04u-%02u-%02u %02u:%02u:%02u\n",
		dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
	auto dbPath = util::FileSystem::RemoveTrailingSlashes(data.GetBasePath());
	report += util::Format("Path: \"%s\"\n", util::ToAnsi(dbPath).c_str());
	if (hasOldFormat)
	{
		report += "\nWARNING: Some of database files have outdated format.\n"
			"  Information below might be inaccurate!\n";
	}
	report += "\n***\n\n";

	report += "General statistics:\n";
	report += "-------------------\n";
	report += util::Format("  Total CPU time spent:      %s\n", FormatCPUTime(common.CPUTimeSpent).c_str());
	report += util::Format("  Numbers saved in database: %s\n", SeparateWithCommas(common.savedNumberC).c_str());
	//report += util::Format("  Total delayed palindromes: %s\n", SeparateWithCommas(common.allPalindromeC).c_str());
	report += util::Format("  Total numbers tested:      %s\n", SeparateWithCommas(common.iterationC).c_str());
	report += util::Format("  Highest tested number:     %s\n", SeparateWithCommas(lastNum).c_str());
	report += util::Format("  Highest step discovered:   %u\n", common.highestStep);
	report += "\n";
	report += util::Format("  Total amount of files:     %s\n", SeparateWithCommas(common.fileC).c_str());
	report += util::Format("  Total database size:       %s\n", FormatSize(common.totalFileSize).c_str());
	report += util::Format("  Average file size:         %s\n", FormatSize(common.totalFileSize / common.fileC).c_str());
	report += util::Format("  Average compression ratio: %.1f%%\n", 100.f * common.totalCDataSize / common.totalDataSize);
////
	for (size_t i = 1; i < Const::MAX_DIGIT_C; ++i)
	{
		const auto& info = rangeA[i];
		if (info.fileC)
		{
			float allLychRatio;
			if (i <= 10)
			{
				uint64_t numC = 9;
				for (size_t j = 1; j < i; ++j, numC *= 10);
				allLychRatio = 100.f * info.allLychrelC.AsI64() / numC;
			} else
			{
				// Точность до 5 знаков после запятой
				auto allLychC = info.allLychrelC.AsString();
				if (allLychC.size() < i)
					allLychC.insert(0, i - allLychC.size(), '0');
				allLychRatio = atoi(allLychC.substr(0, 8).c_str()) / 9.e+5f;
			}

			report += util::Format((i > 3) ? "\n%u-digit range:\n" : "\nNumbers 1-999:\n", i);
			report += util::Format("%s-------------\n", (i < 10) ? "-" : "--");
			report += util::Format("  CPU time spent:            %s\n", FormatCPUTime(info.CPUTimeSpent).c_str());
			report += util::Format("  Delayed palindromes saved: %s\n", SeparateWithCommas(info.savedNumberC).c_str());
			report += util::Format("    Low threshold (steps):   %u\n", info.minSavedStep);
			report += util::Format("    Most delayed (steps):    %u\n", info.highestStep);
			report += util::Format("  Min. search depth (steps): %u\n", info.searchDepth);
			report += util::Format(info.isIncomplete ? "  Total numbers tested:      %s (%.3f%%)\n" :
				"  Total numbers tested:      %s\n", SeparateWithCommas(info.iterationC).c_str(),
				GetRangeProgress(i, info.iterationC));
			report += util::Format("  Primary Lychrels:          %s (%.3f%%)\n",
				SeparateWithCommas(info.primLychrelC).c_str(), 100.f * info.primLychrelC / info.iterationC);
			report += util::Format("  Total Lychrels:            %s (%.3f%%)\n",
				SeparateWithCommas(info.allLychrelC).c_str(), allLychRatio);
			if (info.isIncomplete)
				report += "  WARNING: This range isn't fully verified yet!\n";
		}
	}
////
	bool hasPalindromes = false;
	for (unsigned step = 1; step < Const::MAX_STEP; ++step)
	{
		if (palindromeA[step].count)
		{
			hasPalindromes = true;
			break;
		}
	}
	if (hasPalindromes)
	{
		report += "\n***\n\n";
		report += "Detailed information on each step discovered:\n";
		report += "---------------------------------------------\n";

		for (unsigned step = 1; step < Const::MAX_STEP; ++step)
		{
			if (!palindromeA[step].count)
				continue;

			report += util::Format("\nStep %u:\n", step);
			report += util::Format("  Numbers saved in database:  %s\n",
				SeparateWithCommas(palindromeA[step].count).c_str());
			report += util::Format("  Lowest delayed palindrome:  %s\n",
				SeparateWithCommas(palindromeA[step].lowestNumber).c_str());
			report += util::Format("    Range (length):           %u\n",
				palindromeA[step].lowestNumber.GetLength());
			report += util::Format("    Resulting palindome:      %s\n",
				palindromeA[step].palindrome.AsString().c_str());
			report += util::Format("    Palindrome length:        %u digit(s)\n",
				palindromeA[step].palindrome.GetLength());
			report += util::Format("  Highest delayed palindrome: %s\n",
				SeparateWithCommas(palindromeA[step].highestNumber).c_str());
		}

		std::string notFound;
		unsigned prevNotFoundStep = 0;
		for (unsigned step = 1; step < Const::MAX_STEP; ++step)
		{
			if (palindromeA[step].count)
			{
				if (prevNotFoundStep)
				{
					if (!notFound.empty())
						notFound += ", ";
					if (prevNotFoundStep + 2 <= step)
					{
						notFound += util::Format((prevNotFoundStep + 2 == step) ?
							"%u, %u" : "%u-%u", prevNotFoundStep, step - 1);
					}
					else
						notFound += util::Format("%u", prevNotFoundStep);
				}
				prevNotFoundStep = 0;
			}
			else if (!prevNotFoundStep)
				prevNotFoundStep = step;
		}
		if (prevNotFoundStep)
			notFound += util::Format(notFound.empty() ? "%u-..." : ", %u-...", prevNotFoundStep);
		if (!notFound.empty())
			report += "\nSteps for which no delayed palindromes were found:\n  " + notFound + "\n";
	}
////
	util::BufferedFile out;
	std::wstring filePath = data.GetBasePath() + L"report.txt";
	if (out.Open(filePath, util::FILE_OPEN_WRITE | util::FILE_CREATE_ALWAYS))
	{
		out.Write(report.c_str(), report.size());
		EventManager::PublishEvent("Report has been saved to <DBPath>/report.txt");
		out.Close();
	}
}

//----------------------------------------------------------------------------------------------------------------------
static bool OnFileNotLoaded(const DBChunk* pChunk)
{
	EventManager::PublishEvent(util::Format("#12Failed to load file %s, aborting...",
		util::ToAnsi(pChunk->GetFilePath()).c_str()));
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
static bool AnalyseDBChunk(DataBase& data, RangeInfo& info, PalindromeInfo* palindromeA, DBChunk* pChunk)
{
	++info.fileC;
	info.totalFileSize += pChunk->GetFileSize();
	info.totalDataSize += pChunk->GetDataSize();
	info.totalCDataSize += pChunk->GetCDataSize();

	info.CPUTimeSpent += pChunk->GetCPUTimeSpent();

	info.iterationC += pChunk->GetIterationC();
	info.primLychrelC += pChunk->GetPrimaryLychrelC();
	info.allLychrelC += pChunk->GetAllLychrelC();

	if (unsigned minSavedStep = pChunk->GetMinSavedStep())
	{
		if (!info.minSavedStep || minSavedStep < info.minSavedStep)
			info.minSavedStep = minSavedStep;
	}
	if (unsigned searchDepth = pChunk->GetSearchDepth())
	{
		if (!info.searchDepth || searchDepth < info.searchDepth)
			info.searchDepth = searchDepth;
	}

	if (!pChunk->LoadData(data, DBChunkState::WITHSTATS))//FULLDATA))
		return OnFileNotLoaded(pChunk);

	const unsigned* numCountA = pChunk->GetNumCountA();
	for (unsigned step = 1; step < Const::MAX_STEP; ++step)
	{
		if (numCountA[step])
		{
			info.savedNumberC += numCountA[step];
			if (!info.lowestStep || step < info.lowestStep)
				info.lowestStep = step;
			if (!info.highestStep || step > info.highestStep)
				info.highestStep = step;
		}
	}

	if (info.last < pChunk->GetLast())
		info.last = pChunk->GetLast();

	/*BigNumber num;
	pChunk->SortNumbers();
	for (auto& item : pChunk->GetNumbers())
	{
		//num = item.num;
		//info.allPalindromeC += 1 + num.GetKinNumberC();

		PalindromeInfo& palInfo = palindromeA[item.step];
		++palInfo.count;

		num = item.num;
		if (!palInfo.lowestNumber)
		{
			palInfo.lowestNumber = num;
			palInfo.highestNumber = num;
			num.ReverseAndAdd(item.step);
			palInfo.palindrome = num;
		}
		else if (num > palInfo.highestNumber)
			palInfo.highestNumber = num;
	}*/

	info.hasOldFormat |= pChunk->HasOldFormat();

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
static bool AnalyseDBChunks(DataBase& data, size_t totalChunkC)
{
	Number first, last;
	size_t curRange = 0;
	RangeInfo rangeA[Const::MAX_DIGIT_C + 1];
	PalindromeInfo palindromeA[Const::MAX_STEP + 1];
	for (size_t i = 1; i < Const::MAX_DIGIT_C; ++i)
		rangeA[i].digitC = i;
	bool result = true;

	size_t chunkC = 0;
	uint32_t lastTick = 0;
	data.ForEachChunk([&](DBChunk* pChunk) {
		++chunkC;
		result = false;
		uint32_t tick = ::GetTickCount();
		if (tick - lastTick > 250)
		{
			lastTick = tick;
			auto filePath = pChunk->GetFilePath();
			aux::Printf("\rParsing file #15#%s#7 [%u/%u], %.1f%% done...",
				util::ToAnsi(util::FileSystem::ExtractFilename(filePath)).c_str(),
				chunkC, totalChunkC, 100.f * (chunkC - 1) / totalChunkC);

			if (util::SystemConsole::Instance().IsCtrlCPressed())
			{
				aux::Printf("\b\b\b, #12cancelled...\n");
				return false;
			}
		}

		if (!pChunk->LoadData(data, DBChunkState::HEADERONLY))
			return OnFileNotLoaded(pChunk);

		first = pChunk->GetFirst();
		if (pChunk->GetLast().GetLength() > Const::MAX_DIGIT_C)
			return false;

		Number next = last + 1u;
		if (first > next)
		{
			if (last && last.GetLength() == next.GetLength())
				rangeA[curRange].isIncomplete = true;
			if ((first - 1u).GetLength() == first.GetLength())
				rangeA[pChunk->GetLast().GetLength()].isIncomplete = true;
		}
		last = pChunk->GetLast();
		curRange = last.GetLength();
		result = AnalyseDBChunk(data, rangeA[curRange], palindromeA, pChunk);
		pChunk->UnloadData(DBChunkState::DATAUNLOADED);
		return result;
	});

	//last = rangeA[curRange].last;
	if (curRange && last.GetLength() == (last + 1u).GetLength())
		rangeA[curRange].isIncomplete = true;

	if (result)
		OutputAnalysisResults(data, rangeA, palindromeA);
	return result;
}

//----------------------------------------------------------------------------------------------------------------------
static bool AnalyseDataBase(bool detailsOnNumbers = false)
{
	DataBase data;
	if (!data.Init(false, DBChunkState::WITHSTATS))
	{
		aux::Print("Database not found, exiting...\n");
		return false;
	}

	SystemLog::SetPath(data.GetBasePath() + L"log.txt");
	DBMode::PrintDataBasePath(data.GetBasePath(), 46);

	const uint32_t startTime = ::GetTickCount();
	EventManager::PublishEvent("Database loaded, starting file analysis...");

	Number last, first;
	size_t totalChunkC = 0;
	bool hasOverlaps = false;

	data.ForEachChunk([&](DBChunk* pChunk) {
		first = pChunk->GetFirst();
		if (first <= last)
		{
			EventManager::PublishEvent("#6Overlapping regions detected. Please, run #14--update#6 first");
			hasOverlaps = true;
			return false;
		}
		last = pChunk->GetLast();
		++totalChunkC;
		return true;
	});

	if (!hasOverlaps && totalChunkC && AnalyseDBChunks(data, totalChunkC))
	{
		const uint32_t endTime = GetTickCount();
		aux::Printf("Time in work: %.1fs\n", .001f * (endTime - startTime));
	}

	SystemLog::Instance().Close();
	return !hasOverlaps;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   AnalyseDBMode - анализ базы данных и сбор статистики (режим работы программы)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class AnalyseDBMode final : public Mode
{
public:
	virtual bool Run() override
	{
		// Команда "stats" не имеет параметров
		if (m_Params.size() != 1)
		{
			OnInvalidCmdLine();
			return false;
		}

		return AnalyseDataBase();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   HelpMode - вывод справки об использовании (режим работы программы)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class HelpMode final : public Mode
{
public:
	virtual bool Run() override
	{
		return true;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ShrinkDB - удаление палиндромов с низкими шагами из БД для уменьшения её размера
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
/*static*/ void ShrinkDB()
{
	// Шаги, начиная с которых отложенные палиндромы будут сохраняться в БД (для каждого диапазона чисел).
	// При значении 1 будут сохрнаяться абсолютно все найденные палиндромы. Чем ниже значение, тем больше
	// размер файлов. В основной БД ограничения начинаются с 13-значных чисел: 10, 15, 35, 40, 40, 50...
	static const unsigned stepA[Const::MAX_DIGIT_C + 1] = { 0,
		  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,		//  1 - 10
		  1,   1,  10,  15,  40,  45,  70,  75,  95, 100,		// 11 - 20
		115, 120, 120, 120, 120, 120, 120, 120, 120, 120 };		// 21 - 30

	DataBase data;
	if (data.Init(false, DBChunkState::HEADERONLY))
	{
		uint64_t totalPalCountA[Const::MAX_STEP + 1];
		AML_FILLA(totalPalCountA, 0, Const::MAX_STEP + 1);

		uint32_t lastTick = 0;
		size_t fileC = 0, modifiedC = 0;
		data.ForEachChunk([&](DBChunk* pChunk) {
			if (!pChunk->LoadData(data, DBChunkState::FULLDATA))
			{
				fileC = 0;
				aux::Printc("\r#12Error: failed to load database file");
				return false;
			}

			const size_t range = pChunk->GetLast().GetLength();
			if (range > 3 && range <= Const::MAX_DIGIT_C && stepA[range] > 1)
			{
				if (pChunk->RemovePalindromes(stepA[range]))
				{
					data.SetActiveChunk(pChunk);
					data.Save(0u, 0, 0);
					++modifiedC;
				}
			}

			Number num;
			for (const auto& item : pChunk->GetNumbers())
			{
				num = item.num;
				totalPalCountA[item.step] += 1 + num.GetKinNumberC();
			}

			++fileC;
			pChunk->UnloadData(DBChunkState::HEADERONLY);

			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 250)
			{
				aux::Printf("\rFiles resized: %u/%u", modifiedC, fileC);
				lastTick = tick;
			}
			return true;
		});
		aux::Printf("\rFiles resized: %u/%u\n", modifiedC, fileC);

		fileC = 0;
		lastTick = 0;
		modifiedC = 0;
		bool isSaved = true;
		unsigned dataSize = 0;
		DBChunk* pActiveChunk = nullptr;
		std::vector<DBChunk*> removeList;
		data.ForEachChunk([&](DBChunk* pChunk) {
			if (!pChunk->LoadData(data, DBChunkState::FULLDATA))
			{
				fileC = 0;
				aux::Printc("\r#12Error: failed to load database file");
				return false;
			}

			if (pActiveChunk && pActiveChunk->GetLast() + 1u == pChunk->GetFirst() &&
				pActiveChunk->GetLast().GetLength() == pChunk->GetFirst().GetLength() &&
				dataSize + pChunk->GetDataSize() <= Const::DATA_SAVE_SIZE)
			{
				isSaved = false;
				pActiveChunk->Append(pChunk);
				dataSize += pChunk->GetDataSize();
				if (dataSize > Const::DATA_SAVE_SIZE - Const::DATA_SAVE_SIZE / 8 ||
					dataSize + 2 * pChunk->GetDataSize() > Const::DATA_SAVE_SIZE)
				{
					data.Save(0u, 0, 0);
					dataSize = pActiveChunk->GetDataSize();
					isSaved = true;
				}
				++modifiedC;

				pChunk->UnloadData(DBChunkState::DATAUNLOADED);
				removeList.push_back(pChunk);
			} else
			{
				if (!isSaved)
				{
					data.Save(0u, 0, 0);
					isSaved = true;
				}
				data.SetActiveChunk(pChunk);
				dataSize = pChunk->GetDataSize();

				if (pActiveChunk)
					pActiveChunk->UnloadData(DBChunkState::HEADERONLY);
				pActiveChunk = pChunk;
			}

			++fileC;
			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 250)
			{
				aux::Printf("\rFiles combined: %u/%u", modifiedC, fileC);
				lastTick = tick;
			}
			return true;
		});
		if (fileC && !isSaved)
			data.Save(0u, 0, 0);
		aux::Printf("\rFiles combined: %u/%u\n", modifiedC, fileC);

		fileC = 0;
		lastTick = 0;
		for (DBChunk* pChunk : removeList)
		{
			++fileC;
			data.RemoveChunk(pChunk);
			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 250)
			{
				aux::Printf("\rFiles removed: %u", fileC);
				lastTick = tick;
			}
		}
		aux::Printf("\rFiles removed: %u\n", fileC);

		aux::Print("---\n");
		constexpr unsigned halfSteps = 60;
		for (unsigned step = 1; step <= halfSteps; ++step)
		{
			aux::Printf("Step %3u: %18s   #8|#7   Step %3u: %18s\n", step,
				SeparateWithCommas(totalPalCountA[step]).c_str(), step + halfSteps,
				SeparateWithCommas(totalPalCountA[step + halfSteps]).c_str());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Главная функция
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static int Main(int argC, const wchar_t* argA[])
{
	const std::string buildVer = GetAppVersion();
	aux::Printf("Most Delayed Palindromic Number project. Built on %s\n", buildVer.c_str());
	aux::Print("For more information, please visit us at http://dmaslov.name/mdpn/\n");

	if (!TestFacility::CheckRequirements(false))
	{
		aux::Printc("#12Error: #7failed to pass one or more crucial checks\n");
		return 1;
	}

	LargeMemPages::Init();

	auto mode = Mode::Create(argC, argA);

	if (mode->IsCommand("new", true))
		mode = mode->Expand<SearchMode>();
	else if (mode->IsCommand("check"))
		mode = mode->Expand<CheckDBMode>();
	else if (mode->IsCommand("update"))
		mode = mode->Expand<UpdateDBMode>();
	else if (mode->IsCommand("stats"))
		mode = mode->Expand<AnalyseDBMode>();
	else if (mode->IsCommand("help"))
		mode = mode->Expand<HelpMode>();

	return mode->Run() ? 0 : 1;
}

//----------------------------------------------------------------------------------------------------------------------
int wmain(int argC, const wchar_t* argA[])
{
	return GuardedCall(std::bind(Main, argC, argA), 1);
}
