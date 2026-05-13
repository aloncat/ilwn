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
//   ListAllPalindromes - анализ базы данных и перечисление всех палиндромов для шагов 230 и более
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
	return util::Format("PAL[%u] %s [%u][%u] %s", palindrome.GetLength(),
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
		if (long long fileSize = f.GetSize(); fileSize > 0 && fileSize <= 1024 * 1024)
		{
			size_t dataSize = static_cast<size_t>(fileSize);
			char* buffer = new char[dataSize];

			if (f.Read(buffer, dataSize))
			{
				isLoaded = true;

				Number num;
				std::string s;
				for (const char* p = buffer; dataSize;)
				{
					size_t pad = 0, len = GetStringLength(p, dataSize);
					while (pad < len && p[len - pad - 1] == 10 || p[len - pad - 1] == 13)
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

						Palindrome pal(num);
						allPalindromes.insert(pal);
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
		for (const Palindrome* p : sorted)
		{
			auto s = p->ToString() + '\n';
			f.Write(s.c_str(), s.size());
		}
		f.SaveTo(L"results.txt");
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static bool ListAllPalindromes(std::set<Palindrome>& allPalindromes)
{
	constexpr unsigned STEP_OF_INTEREST = 230;

	DataBase data;
	if (data.Init(false, DBChunkState::HEADERONLY))
	{
		size_t fileC = 0;
		uint32_t lastTick = 0;
		data.ForEachChunk([&](DBChunk* pChunk) {
			if (!pChunk->LoadData(data, DBChunkState::WITHSTATS))
			{
				aux::Printc("\r#12Error\n");
				return false;
			}

			if (pChunk->GetHighestStep() >= STEP_OF_INTEREST)
			{
				if (!pChunk->LoadData(data, DBChunkState::FULLDATA))
				{
					aux::Printc("\r#12Error\n");
					return false;
				}

				Number num;
				for (const auto& item : pChunk->GetNumbers())
				{
					if (item.step >= STEP_OF_INTEREST)
					{
						num = item.num;
						Palindrome pal(num);
						if (pal.palindrome.GetLength() >= 100)
						{
							aux::Printf("\r%s\n", pal.ToString().c_str());

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
					}
				}
			}

			pChunk->UnloadData(DBChunkState::DATAUNLOADED);
			++fileC;

			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 500)
			{
				aux::Printf("\rFiles processed: %u", fileC);
				lastTick = tick;
			}

			return true;
		});

		SaveResults(allPalindromes);
		aux::Printf("\rTask finished. Files processed: %u\n", fileC);
	}

	return 0;
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
	if (util::FileSystem::FileExists(L"results.txt") && !LoadResults(allPalindromes))
	{
		aux::Print("Couldn't load results, file format is invalid\n");
		return 1;
	}

	if (!ListAllPalindromes(allPalindromes))
		return 1;

	return 0;
}
