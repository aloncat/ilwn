//∙MDPN (List)
#include "pch.h"

#include "dbase.h"
#include "number.h"
#include "test.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/file.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/winapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Анализ базы данных и перечисление всех палиндромов для шагов 230 и более
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr unsigned STEP_OF_INTEREST = 230;

//--------------------------------------------------------------------------------------------------------------------------------
struct Palindrome
{
	Number number;			// Наименьшее число, разрешающееся за steps шагов
	Number palindrome;		// Результирующий палиндром
	unsigned steps = 0;		// Количество итераций

public:
	Palindrome(const Number& num);

	bool operator <(const Palindrome& that) const;
	std::string ToString() const;
};

//--------------------------------------------------------------------------------------------------------------------------------
Palindrome::Palindrome(const Number& num)
{
	number = num;
	BigNumber bigNum = num;
	if (!bigNum.RAATillPalindrome(500, steps))
		throw std::exception();
	palindrome = bigNum;
}

//--------------------------------------------------------------------------------------------------------------------------------
inline bool Palindrome::operator <(const Palindrome& that) const
{
	return palindrome < that.palindrome;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string Palindrome::ToString() const
{
	return util::Format("PAL[%03u] %s [%u][%u] %s", palindrome.GetLength(),
		palindrome.AsString().substr(0, 20).c_str(), steps, number.GetLength(),
		SeparateWithCommas(number).c_str());
}

//--------------------------------------------------------------------------------------------------------------------------------
static inline size_t GetStringLength(const char* buffer, size_t size)
{
	for (size_t len = 0; len < size; ++len)
	{
		if (buffer[len] == 10)
			return len + 1;
	}
	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
static bool LoadResults(std::set<Palindrome>& allPalindromes)
{
	bool isLoaded = false;
	if (util::BinaryFile f; f.Open(L"results.txt", util::FILE_OPEN_READ))
	{
		if (long long fileSize = f.GetSize(); fileSize == 0)
		{
			// Пустой файл ошибкой не считается
			isLoaded = true;
		}
		else if (fileSize > 0 && fileSize <= 1024 * 1024)
		{
			size_t dataSize = static_cast<size_t>(fileSize);
			char* buffer = new char[dataSize];

			if (f.Read(buffer, dataSize))
			{
				isLoaded = true;

				Number num;
				BigNumber bigNum;
				std::string s;

				for (const char* p = buffer; dataSize;)
				{
					size_t pad = 0, len = GetStringLength(p, dataSize);
					while (pad < len && (p[len - pad - 1] == 10 || p[len - pad - 1] == 13))
						++pad;

					s.assign(p, len - pad);
					if (util::TrimInplace(s); !s.empty())
					{
						auto tokens = util::Split(s, " ");
						if (tokens.size() < 4 || !ConvertToNumber(util::FromAnsi(tokens[3]), num))
						{
							isLoaded = false;
							break;
						}

						bigNum = num;
						unsigned steps;
						if (!bigNum.RAATillPalindrome(500, steps) || steps < 200)
						{
							isLoaded = false;
							break;
						}

						Palindrome pal(num);
						if (auto it = allPalindromes.find(pal); it == allPalindromes.end())
						{
							allPalindromes.insert(std::move(pal));
						}
						else if (pal.steps > it->steps || (pal.steps == it->steps && pal.number < it->number))
						{
							allPalindromes.erase(it);
							allPalindromes.insert(std::move(pal));
						}
					}

					p += len;
					dataSize -= len;
				}
			}

			delete[] buffer;
		}

		f.Close();
	}

	return isLoaded;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void SaveResults(const std::set<Palindrome>& allPalindromes)
{
	if (allPalindromes.empty())
		return;

	std::vector<const Palindrome*> sorted;
	for (const Palindrome& pal : allPalindromes)
		sorted.push_back(&pal);

	std::sort(sorted.begin(), sorted.end(), [](const Palindrome* a, const Palindrome* b) {
		return a->palindrome.GetLength() < b->palindrome.GetLength() ||
			(a->palindrome.GetLength() == b->palindrome.GetLength() &&
				a->palindrome < b->palindrome);
	});

	if (util::MemoryFile f; f.Open())
	{
		// Основная группа палиндромов, которая нас интересует. С ними
		// мы продолжим работу с помощью алгоритма обратного вывода
		for (const Palindrome* p : sorted)
		{
			if (p->steps >= STEP_OF_INTEREST)
			{
				auto s = p->ToString() + '\n';
				f.Write(s.c_str(), s.size());
			}
		}

		f.Write("\n", 1);

		// Дополнительная группа палиндромов, разрешающихся за меньшее количество шагов. Их мы тоже подвергнем
		// обработке алгоритмом обратного вывода. И если он найдёт среди них родственные отложенные палиндромы,
		// требующие STEP_OF_INTEREST шагов или больше, то позже мы их включим в основную группу
		for (const Palindrome* p : sorted)
		{
			if (p->steps < STEP_OF_INTEREST)
			{
				auto s = p->ToString() + '\n';
				f.Write(s.c_str(), s.size());
			}
		}

		f.SaveTo(L"results.txt");
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static void ListAllPalindromes(std::set<Palindrome>& allPalindromes)
{
	DataBase data;
	if (data.Init(false, DBChunkState::HEADERONLY))
	{
		size_t fileCount = 0;
		size_t newPalCount = 0;
		uint32_t lastTick = 0;

		static bool error = false;
		data.ForEachChunk([&](DBChunk* chunk) {
			if (!chunk->LoadData(data, DBChunkState::WITHSTATS))
			{
				error = true;
				aux::Printc("\r#12Error loading database chunk\n");
				return false;
			}

			constexpr size_t LOWEST_STEP = STEP_OF_INTEREST - 10;
			if (chunk->GetHighestStep() >= LOWEST_STEP)
			{
				if (!chunk->LoadData(data, DBChunkState::FULLDATA))
				{
					error = true;
					aux::Printc("\r#12Error loading database chunk\n");
					return false;
				}

				Number num;
				for (const auto& item : chunk->GetNumbers())
				{
					if (item.step >= LOWEST_STEP)
					{
						num = item.num;
						Palindrome pal(num);
						if (auto it = allPalindromes.find(pal); it == allPalindromes.end())
						{
							aux::Printf("\r%s\n", pal.ToString().c_str());
							allPalindromes.insert(std::move(pal));
							++newPalCount;
						}
						else if (pal.steps > it->steps || (pal.steps == it->steps && pal.number < it->number))
						{
							aux::Printf("\r%s\n", pal.ToString().c_str());
							allPalindromes.erase(it);
							allPalindromes.insert(std::move(pal));
						}
					}
				}
			}

			chunk->UnloadData(DBChunkState::DATAUNLOADED);
			++fileCount;

			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 500)
			{
				aux::Printf("\rFiles processed: %u", fileCount);
				lastTick = tick;
			}

			return true;
		});

		if (!error)
		{
			SaveResults(allPalindromes);
			aux::Printf("\rNew palindromes found: %u, total: %u\n", newPalCount, allPalindromes.size());
			aux::Printf("Task done. Files processed: %u\n", fileCount);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
int ListAllPalindromesMain()
{
	if (!TestFacility::CheckRequirements(false))
	{
		aux::Printc("#12Error: #7failed to pass one or more crucial checks\n");
		return 1;
	}

	std::set<Palindrome> allPalindromes;
	if (util::FileSystem::FileExists(L"results.txt"))
	{
		if (!LoadResults(allPalindromes))
		{
			aux::Print("Couldn't load results, file format is invalid\n");
			return 1;
		}

		aux::Printf("Previous results loaded: %u palindrome(s) found\n", allPalindromes.size());
	}

	ListAllPalindromes(allPalindromes);
	return 0;
}
