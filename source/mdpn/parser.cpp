//∙MDPN
#include "pch.h"

#include "const.h"
#include "number.h"
#include "util.h"

#include <core/array.h>
#include <core/auxutil.h>
#include <core/file.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/winapi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CheckDelayedPalindrome - проверка отложенного палиндрома и вывод шага-за-шагом
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: перенести этот код в Main - сделать возможность его использования по ключу
// командной строки (аналогично параметру new, вторым параметром передавать число)

//----------------------------------------------------------------------------------------------------------------------
void CheckDelayedPalindrome(const BigNumber& num, bool printAllSteps, bool printPalindrome = true)
{
	if (!num)
	{
		aux::Printc("Given number: #15#0#7\nThis number is not a natural number\n");
		return;
	}

	BigNumber pal(num);
	unsigned stepDoneC = 0;
	if (!pal.RAATillPalindrome(1000, stepDoneC))
	{
		aux::Printf("Given number: #15#%s#7\nThis number is most probably a Lychrel number\n",
			num.AsString().c_str());
		return;
	}

	aux::Printf("Given number:    #15#%s\n", num.AsString().c_str());
	aux::Printf("Steps performed: #15#%u\n", stepDoneC);

	if (printAllSteps)
	{
		aux::Printc("#14Step by step:\n");
		size_t maxLen = pal.GetLength();
		pal = num;

		for (unsigned i = 0; i < stepDoneC; ++i)
		{
			aux::Printf(util::Format("#15#%%3u#7: #%d#%%%ds#7 + %%%ds\n", i ? 7 : 15, maxLen, maxLen).c_str(),
				i + 1, pal.AsString().c_str(), pal.GetReversed().AsString().c_str());
			pal.ReverseAndAdd();
		}
		std::string s(2 * maxLen + 3, '-');
		aux::Printf("     %s\n     #10#%s\n", s.c_str(), pal.AsString().c_str());
	}
	else if (printPalindrome)
	{
		aux::Printf("Palindrome:      #15#%s\n", pal.AsString().c_str());
	}
}

//----------------------------------------------------------------------------------------------------------------------
void CheckDelayedPalindrome(const char* pStr, bool printAllSteps, bool printPalindrome = true)
{
	if (!pStr || !pStr[0])
		return;

	if (!IsNumber(pStr))
		aux::Printf("#12Error: \"%s\" is not a valid number\n", pStr);
	else
		CheckDelayedPalindrome(BigNumber(pStr), printPalindrome, printAllSteps);
}

//----------------------------------------------------------------------------------------------------------------------
void CheckDelayedPalindrome(const std::string& str, bool printAllSteps, bool printPalindrome = true)
{
	CheckDelayedPalindrome(str.c_str(), printAllSteps, printPalindrome);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ParseData - сбор статистики по файлам базы данных
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static void GenRecordHtml(const uint64_t* counterA, const std::vector<BigNumber>& lowestA)
{
	util::MemoryFile f;
	f.Open();

	std::string s;
	bool hadRecords = false;
	const unsigned MAX_LEN = 30;
	for (size_t len = 1; len <= MAX_LEN; ++len)
	{
		bool hasRecords = false;
		for (unsigned step = 1; step <= Const::MAX_STEP; ++step)
		{
			if (counterA[step])
			{
				unsigned stepDoneC = 0;
				BigNumber pal = lowestA[step];
				if ((pal.GetLength() == len) && pal.RAATillPalindrome(Const::MAX_STEP, stepDoneC) &&
					(stepDoneC == step))
				{
					if (hadRecords) f.Write("\n", 1);
					hadRecords = false;
					hasRecords = true;

					s = util::Format("%s%s%s%u%s%u%s%s%s\n",
						"  <tr class=\"recnum\"><td align=right>", SeparateWithCommas(lowestA[step]).c_str(),
						"&nbsp;</td><td align=center>", step,
						"</td><td></td><td align=center>", pal.GetLength(),
						"</td><td>&nbsp;", pal.AsString().c_str(),
						"</td></tr>");
					f.Write(s.c_str(), s.size());
				}
			}
		}
		hadRecords |= hasRecords;
	}

	f.SaveTo(L"records_html.txt");
	f.Close();
}

//----------------------------------------------------------------------------------------------------------------------
static bool ParseDataFile(util::MemoryFile& data, uint64_t& totalCPUTime, BigNumber& lastNum, uint64_t* counterBaseA,
	uint64_t* counterA, std::vector<BigNumber>& lowestA, std::vector<BigNumber>& highestA)
{
	char header[512];
	if (!data.Read(header, 512))
		return false;

	std::string s;
	BigNumber num;
	size_t statSize = 0;
	size_t dataSize = 0;
	const char* p = header;
	const char* pEnd = p + 512;
	for (size_t len, next = 0; p < pEnd; p += next)
	{
		len = 0; while ((p + len < pEnd) && (p[len] != 0x0a) && (p[len] != 0x0d)) ++len;
		next = len + 1; while ((p + next < pEnd) && ((p[next] == 0x0a) || (p[next] == 0x0d))) ++next;
		if (!len || (p[0] == '#')) break;

		s = util::LoCase(std::string(p, len));
		if (s.substr(0, 5) == "last:")
		{
			BigNumber n(s.c_str() + 5);
			if (n > lastNum) lastNum = n;
		}
		else if (s.substr(0, 9) == "cpuspent:")
		{
			if (IsNumber(s.c_str() + 9))
				totalCPUTime += atoi(s.c_str() + 9);
		}
		else if (s.substr(0, 6) == "dsize:")
		{
			if (IsNumber(s.c_str() + 6))
				dataSize = atoi(s.c_str() + 6);
		}
		else if (s.substr(0, 6) == "ssize:")
		{
			if (IsNumber(s.c_str() + 6))
				statSize = atoi(s.c_str() + 6);
		}
	}

	size_t currentRange = lastNum.GetLength();
	if ((lastNum + 1u).GetLength() > currentRange)
		++currentRange;

	long long fileSize = data.GetSize();
	long long compressedSize = fileSize - 512 - statSize - 5;
	if (compressedSize <= 0) return (compressedSize == 0);
	if (dataSize == 0) return false;
	data.SetPosition(fileSize - 5);
	data.Truncate();

	util::MemoryFile uData;
	uData.Open();

	data.SetPosition(512 + statSize);
	if (!DecompressFile(data, uData)) return false;
	size_t uncompressedSize = (size_t) uData.GetSize();
	if (uncompressedSize != dataSize) return false;
	uData.SetPosition(0);

	util::DynamicArray<char> buffer(uncompressedSize);
	if (!uData.Read(buffer, uncompressedSize)) return false;

	p = buffer;
	pEnd = p + uncompressedSize;
	for (size_t len, next = 0; p < pEnd; p += next)
	{
		len = 0; while ((p + len < pEnd) && (p[len] != 0x0a) && (p[len] != 0x0d)) ++len;
		next = len + 1; while ((p + next < pEnd) && ((p[next] == 0x0a) || (p[next] == 0x0d))) ++next;
		if (!len || (p[0] == '#')) continue;

		size_t step = 0, i = len - 1;
		for (size_t k = 1; i && (p[i] >= '0') && (p[i] <= '9'); --i, k *= 10) step += k * (p[i] - 48);
		if ((i == 0) || (p[i] != 32) || (step < 1) || (step > Const::MAX_STEP)) return false;
		s.assign(p, i);
		num.Set(s);

		++counterBaseA[step];
		counterA[step] += 1 + num.GetKinNumberCount();

		if (!lowestA[step] || (num < lowestA[step]))
			lowestA[step] = num;
		if (num > highestA[step])
			highestA[step] = num;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
static std::string FormatNumberCount(uint64_t count)
{
	if (count <= 99999999)
		return SeparateWithCommas(count);
	if (count <= 999999999)
		return util::Format("~%.2f M", 1e-6f * count);
	return util::Format("~%.2f G", 1e-9f * count);
}

//----------------------------------------------------------------------------------------------------------------------
static void PrintDataStats(BigNumber& lastNum, uint64_t totalCPUTime, uint64_t* counterBaseA, uint64_t* counterA,
	std::vector<BigNumber>& lowestA, std::vector<BigNumber>& highestA, bool primaryNumbersOnly)
{
	uint64_t totalSeconds = (totalCPUTime + 500) / 1000;
	unsigned dayC = static_cast<unsigned>(totalSeconds / (24 * 3600));
	totalSeconds -= (24 * 3600) * dayC;
	unsigned hourC = static_cast<unsigned>(totalSeconds / 3600);
	totalSeconds -= 3600 * hourC;
	unsigned minuteC = static_cast<unsigned>(totalSeconds / 60);
	unsigned secondC = totalSeconds % 60;

	uint64_t totalC = 0;
	uint64_t totalBaseC = 0;
	unsigned highestStep = 0;
	for (unsigned i = 0; i <= Const::MAX_STEP; ++i)
	{
		totalC += counterA[i];
		totalBaseC += counterBaseA[i];
		if (counterBaseA[i])
			highestStep = i;
	}

	aux::Print("\nDetails:\n");
	aux::Printf("  Total CPU time spent:            #15#%u:%02u:%02u:%02u\n", dayC, hourC, minuteC, secondC);
	aux::Printf("  Last iteration processed:        #15#%s\n", SeparateWithCommas(lastNum).c_str());
	aux::Printf("  Numbers saved in database:       #15#%s\n", SeparateWithCommas(totalBaseC).c_str());
	aux::Printf("  Total delayed palindromes found: #15#%s\n", FormatNumberCount(totalC).c_str());
	aux::Printf("  Highest step discovered:         #15#%s\n", SeparateWithCommas(highestStep).c_str());
	aux::Print("  ---\n");

	unsigned lo, hi;
	for (lo = 1; lo <= Const::MAX_STEP && !counterBaseA[lo]; ++lo);
	for (hi = Const::MAX_STEP; hi && !counterBaseA[hi]; --hi);

	unsigned clen = 0, lolen = 0, hilen = 0;
	for (unsigned k, i = lo; i <= hi; ++i)
	{
		clen = std::max(clen, static_cast<unsigned>(FormatNumberCount(counterA[i]).size()));
		k = static_cast<unsigned>(lowestA[i].GetLength());
		lolen = std::max(lolen, k + (k - 1) / 3);
		k = static_cast<unsigned>(highestA[i].GetLength());
		hilen = std::max(hilen, k + (k - 1) / 3);
	}
	std::string fmt = util::Format("  #%%dNumbers found for step #%%d#%%3u#%%d: "
		"#%%d#%%%us   #6[#8#%%%us#6]#8  %%%us\n", clen, lolen, hilen);

	for (unsigned i = lo; i <= hi; ++i)
	{
		bool z = !counterBaseA[i];
		aux::Printf(fmt.c_str(), z ? 8 : 7, z ? 8 : 14, i, z ? 8 : 7, z ? 8 : 15,
			FormatNumberCount(primaryNumbersOnly ? counterBaseA[i] : counterA[i]).c_str(),
			!lowestA[i] ? "-" : SeparateWithCommas(lowestA[i]).c_str(),
			!highestA[i] ? "-" : SeparateWithCommas(highestA[i]).c_str());
	}
}

//----------------------------------------------------------------------------------------------------------------------
void ParseData(const std::wstring& path, bool primaryNumbersOnly = false, bool genRecHtml = false)
{
	const unsigned startTime = ::GetTickCount();
	aux::Print("Initializing database...");

	std::vector<std::wstring> fileList;
	const std::wstring dbPath = path.empty() ? L"data/" : path;
	util::FileSystem::GetFileList(dbPath + L"*.db", fileList, true);
	util::FileSystem::GetFileList(dbPath + L"*.pal", fileList, true);

	const size_t fileC = fileList.size();
	aux::Printf(" %u file(s) found\n", fileC);
	if (!fileC)
		return;

	util::DynamicArray<uint64_t> counterA(Const::MAX_STEP + 1);
	AML_FILLA(counterA, 0, Const::MAX_STEP + 1);
	util::DynamicArray<uint64_t> counterBaseA(Const::MAX_STEP + 1);
	AML_FILLA(counterBaseA, 0, Const::MAX_STEP + 1);

	std::vector<BigNumber> highestA, lowestA;
	highestA.resize(Const::MAX_STEP + 1);
	lowestA.resize(Const::MAX_STEP + 1);

	BigNumber lastNum;
	uint64_t totalCPUTime = 0;
	size_t filesProcessedC = 0;
	long long totalFileSize = 0;

	unsigned lastTick = ::GetTickCount();
	for (size_t i = 0; i < fileC; ++i)
	{
		std::wstring filename = util::FileSystem::ExtractFilename(fileList[i]);
		unsigned tick = ::GetTickCount();
		if (tick - lastTick >= 100)
		{
			aux::Printf(L"\rParsing file [%u/%u] %s...", i + 1, fileC, filename.c_str());
			lastTick = tick;
		}
		util::MemoryFile f;
		if (!f.LoadFrom(fileList[i]))
			break;
		if (!ParseDataFile(f, totalCPUTime, lastNum, counterBaseA, counterA, lowestA, highestA))
			break;
		totalFileSize += f.GetSize();
		++filesProcessedC;
	}
	if (filesProcessedC != fileC)
	{
		aux::Printf("\n#12Unexpected error occured. Process aborted\n");
		return;
	}
	unsigned endTime = ::GetTickCount();
	float totalElapsed = .001f * (endTime - startTime);

	aux::Printf("\rParsing files... total %.1f#8MiB#7, done in %.2f#8s#7\n",
		(1.f / (1024 * 1024)) * totalFileSize, totalElapsed);

	PrintDataStats(lastNum, totalCPUTime, counterBaseA, counterA, lowestA, highestA, primaryNumbersOnly);

	if (genRecHtml)
		GenRecordHtml(counterA, lowestA);
}
