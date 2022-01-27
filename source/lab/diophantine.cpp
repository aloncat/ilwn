//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/file.h>
#include <core/strformat.h>
#include <core/winapi.h>

#include <set>

namespace lab {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск коэффициентов Диофантова уравнения вида p.1.n (наивное решение)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Timer
{
public:
	Timer()
	{
		::QueryPerformanceCounter(&m_Start);
	}

	float Elapsed() const
	{
		LARGE_INTEGER finish, frequency;
		::QueryPerformanceCounter(&finish);
		::QueryPerformanceFrequency(&frequency);

		return static_cast<float>(finish.QuadPart - m_Start.QuadPart) / frequency.QuadPart;
	}

protected:
	LARGE_INTEGER m_Start;
};

//--------------------------------------------------------------------------------------------------------------------------------
static std::vector<unsigned> GetPrimes(unsigned limit)
{
	std::vector<unsigned> primes;

	if (limit > 1)
	{
		primes.reserve(100);
		primes.push_back(2);

		if (limit > 2)
		{
			const size_t bufferSize = (limit + 13) >> 4;
			uint8_t* bits = new uint8_t[bufferSize];
			memset(bits, 0, bufferSize);

			const unsigned lastBit = (limit - 3) >> 1;
			const unsigned k = (limit < 25) ? 0 : (static_cast<unsigned>(sqrt(limit)) - 3) >> 1;

			for (unsigned i = 0; i <= k; ++i)
			{
				if (!(bits[i >> 3] & (1 << (i & 7))))
				{
					unsigned n = (i << 1) + 3;
					primes.push_back(n);

					for (unsigned j = (i + 1) * 3; j <= lastBit; j += n)
						bits[j >> 3] |= 1 << (j & 7);
				}
			}

			for (unsigned i = k + 1; i <= lastBit; ++i)
			{
				if (!(bits[i >> 3] & (1 << (i & 7))))
					primes.push_back((i << 1) + 3);
			}

			delete[] bits;
		}
	}

	return primes;
}

//--------------------------------------------------------------------------------------------------------------------------------
struct Solution
{
	// Все коэффициенты уравнения
	std::vector<unsigned> factors;

public:
	Solution(unsigned left, const unsigned* right, int count)
	{
		factors.push_back(left);
		for (int i = 0; i < count; ++i)
			factors.push_back(right[i]);
	}

	void SortFactors()
	{
		std::sort(factors.begin(), factors.end(), [](unsigned left, unsigned right) {
			return left > right;
		});
	}

	bool operator ==(const Solution& rhs) const
	{
		if (const size_t count = factors.size(); count && count == rhs.factors.size())
		{
			for (size_t i = 0; i < count; ++i)
			{
				if (factors[i] != rhs.factors[i])
					return false;
			}
			return true;
		}
		return false;
	}

	bool operator <(const Solution& rhs) const
	{
		if (const size_t count = factors.size(); count && count == rhs.factors.size())
		{
			for (size_t i = 0; i < count; ++i)
			{
				if (factors[i] != rhs.factors[i])
					return factors[i] < rhs.factors[i];
			}
		}
		return false;
	}
};

//--------------------------------------------------------------------------------------------------------------------------------
class Solutions
{
public:
	bool Insert(const Solution& s)
	{
		if (!IsPrimitive(s))
			return false;

		Solution sorted = s;
		sorted.SortFactors();

		auto result = m_Solutions.insert(std::move(sorted));
		return result.second;
	}

	size_t Count() const
	{
		return m_Solutions.size();
	}

protected:
	bool IsPrimitive(const Solution& s)
	{
		if (s.factors.empty())
			return false;

		unsigned lowest = ~0u;
		for (auto f : s.factors)
		{
			if (f < lowest)
				lowest = f;
		}

		if (m_Primes.empty() || m_Primes.back() < lowest)
			m_Primes = GetPrimes(lowest + 5000);

		for (unsigned p : m_Primes)
		{
			if (p > lowest)
				break;

			bool allDivisible = true;
			for (auto f : s.factors)
			{
				if (f % p)
				{
					allDivisible = false;
					break;
				}
			}

			if (allDivisible)
				return false;
		}

		return true;
	}

protected:
	std::set<Solution> m_Solutions;		// Все найденные решения уравнения
	std::vector<unsigned> m_Primes;		// Набор простых чисел (начиная с 2)
};

//--------------------------------------------------------------------------------------------------------------------------------
static unsigned CalcPowers(unsigned high, int power, int count, uint64_t* powers)
{
	memset(powers, 0, 8 * (high + 1));

	for (unsigned i = 1; i <= high; ++i)
	{
		uint64_t o, p = 1, c = i * count;

		for (int j = 0; j < power; ++j)
		{
			o = p;
			p *= i;

			// Проверяем, не возникло ли переполнение при умножении, а также, что оно не возникнет позже при сложении
			// count слагаемых уравнения. Если переполнение возникло, значит для текущего i значение в степени power,
			// умноженное на count, не помещается в 64 бита, и мы должны прервать цикл, вернув предыдущее значение i
			// как максимально возможное значение коэффициента
			if ((p * count) / c != o)
				return i - 1;
		}

		powers[i] = p;
	}

	return high;
}

//--------------------------------------------------------------------------------------------------------------------------------
static bool SearchForFactors(int power, int count, unsigned hiFactor)
{
	constexpr int MAX_COUNT = 50;
	constexpr unsigned MAX_FACTOR = 59999;

	if (power < 1 || power > 9 || count < 2 || count > MAX_COUNT)
		return false;

	util::BinaryFile log;
	log.Open(util::Format(L"log-%i.1.%i.txt", power, count),
		util::FILE_OPEN_ALWAYS | util::FILE_OPEN_READWRITE);

	if (auto fileSize = log.GetSize(); fileSize < 0 || !log.SetPosition(fileSize))
	{
		log.Close();
		aux::Printf("Error: failed to open log file\n");
		return false;
	}

	aux::Printf("Searching for factors (%i.1.%i)\n", power, count);

	uint64_t* powers = new uint64_t[MAX_FACTOR + 1];
	// NB: т.к. сумма слагаемых правой части уравнения не включает последний коэффициент, мы можем передать
	// в функцию значение (count - 1) вместо count, увеличив этим максимально допустимое значение maxFactor
	const unsigned maxFactor = CalcPowers(MAX_FACTOR, power, count - 1, powers);
	aux::Printf("Factor limit is set to #10#%u\n", maxFactor);

	// Массив k хранит коэффициенты правой части уравнения, начиная с индекса 1.
	// В элементе с индексом 0 будем хранить коэффициент левой части уравнения
	unsigned k[1 + MAX_COUNT] = { (hiFactor > 1) ? hiFactor : 2 };
	for (int i = 1; i <= count; ++i)
		k[i] = 1;

	size_t itCount = 0;
	uint32_t lastTick = 0;
	bool isCancelled = false;

	Solutions solutions;
	Timer timer;

	while (k[0] <= maxFactor)
	{
		uint64_t sum = 0;
		// Вычисляем сумму в правой части без последнего коэффициента
		for (int i = 1; i < count; ++i)
			sum += powers[k[i]];

		// Вычисляем значение степени последнего коэффициента правой
		// части, при котором может существовать решение уравнения
		const uint64_t lastFP = powers[k[0]] - sum;

		k[count] = 0;
		// Так как массив степеней отсортирован по возрастанию, используем бинарный поиск,
		// чтобы проверить существование числа, соответствующего значению степени lastFP.
		// Искомое значение не может превышать значения предпоследнего коэффициента
		for (unsigned lo = 1, hi = k[count - 1]; lo <= hi;)
		{
			unsigned mid = (lo + hi) >> 1;
			if (auto v = powers[mid]; v < lastFP)
				lo = mid + 1;
			else if (v > lastFP)
				hi = mid - 1;
			else
			{
				k[count] = mid;
				break;
			}
		}

		// Если значение k[count] != 0, значит, коэффициент для значения степени
		// lastFP существует и мы нашли подходящий набор коэффициентов (решение)
		if (k[count] && solutions.Insert(Solution(k[0], k + 1, count)))
		{
			// Формируем строку с коэффициентами
			auto s = std::to_string(k[0]);
			s += "=" + std::to_string(k[1]);
			for (int i = 2; i <= count; ++i)
			{
				s += "+" + std::to_string(k[i]);
			}

			s += "\n";
			// И выводим её на экран и в файл
			aux::Printf("\rSolution found: %s", s.c_str());
			log.Write(s.c_str(), s.size());

			// Если нашли 100 решений, заканчиваем работу
			if (solutions.Count() >= 100)
				break;
		}

		if (!(++itCount & 0x7fff))
		{
			if (util::SystemConsole::Instance().IsCtrlCPressed())
			{
				isCancelled = true;
				break;
			}
			if (uint32_t tick = ::GetTickCount(); tick - lastTick >= 500)
			{
				lastTick = tick;
				aux::Printf("\rTesting %u=%u+%u...    \b\b\b\b", k[0], k[1], k[2]);
			}
		}

		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
		for (int i = count - 1; i > 0; --i)
		{
			if (k[i] < k[i - 1])
			{
				++k[i];
				break;
			}
			k[i] = 1;
			if (i == 1)
				++k[0];
		}
	}

	aux::Printf("\rTask %s, solutions found: #6#%u\n", isCancelled ?
		"cancelled" : "finished", solutions.Count());
	aux::Printf("Running time: %.2fs\n", timer.Elapsed());

	delete[] powers;

	auto s = util::Format("Highest factor: %u\n", k[0]);
	log.Write(s.c_str(), s.size());
	log.Close();

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
int DiophantineMain(int argCount, const wchar_t* args[])
{
	// Программа принимает 2 обязательных и 1 опциональный аргумент командной строки: 1) степень уравнения,
	// 2) количество слагаемых в его правой части, 3) опциональное значение самого старшего коэффициента
	// (в левой части), с которого будет начат поиск (при его отсутствии будет использовано значение 2)

	if (argCount == 3 || argCount == 4)
	{
		int power = wcstol(args[1], nullptr, 10);
		int count = wcstol(args[2], nullptr, 10);
		int hiFactor = (argCount >= 4) ? wcstol(args[3], nullptr, 10) : 2;

		if (power >= 1 && power <= 9 && count >= 2 && count <= 50 && hiFactor >= 0)
		{
			// Ищем коэффициенты. При ошибке функция вернёт false
			if (SearchForFactors(power, count, hiFactor))
				return 0;

			// Что-то пошло не так. Сообщение об ошибке уже выведено
			return 1;
		}
	}

	// Аргументы в командной строке некорректны
	aux::Print("Error: invalid command line\n");
	return 1;
}

} // namespace lab
