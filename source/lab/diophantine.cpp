//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include <set>

namespace lab {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск коэффициентов Диофантова уравнения вида p.1.n (наивное решение)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
bool SearchForFactors(int power, int count, unsigned hiFactor)
{
	constexpr int MAX_COUNT = 50;
	constexpr unsigned MAX_FACTOR = 49999;

	if (power < 2 || power > 9 || count < 2 || count > MAX_COUNT)
		return false;

	printf("Searching for factors (%i.1.%i)\n", power, count);

	uint64_t* powers = new uint64_t[MAX_FACTOR + 1];
	const unsigned maxFactor = CalcPowers(MAX_FACTOR, power, count, powers);
	printf("Factor limit is set to %u\n", maxFactor);

	// Массив k хранит коэффициенты правой части уравнения, начиная с индекса 1.
	// В элементе с индексом 0 будем хранить коэффициент левой части уравнения
	unsigned k[1 + MAX_COUNT] = { (hiFactor > 1) ? hiFactor : 2 };
	for (int i = 1; i <= count; ++i)
		k[i] = 1;

	Solutions solutions;

	while (k[0] <= maxFactor)
	{
		uint64_t sum = 0;
		// Вычисляем сумму в правой части
		for (int i = 1; i <= count; ++i)
			sum += powers[k[i]];

		// Если сумма в правой части равна степени коэффициента в левой,
		// то значит, мы нашли подходящий набор коэффициентов (решение)
		if (sum == powers[k[0]] && solutions.Insert(Solution(k[0], k + 1, count)))
		{
			// Формируем строку с коэффициентами
			auto s = std::to_string(k[0]);
			s += "=" + std::to_string(k[1]);
			for (int i = 2; i <= count; ++i)
			{
				s += "+" + std::to_string(k[i]);
			}

			// И выводим её на экран
			printf("Solution found: %s\n", s.c_str());

			// Если нашли 100 решений, заканчиваем работу
			if (solutions.Count() >= 100)
				break;
		}

		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
		for (int i = count; i > 0; --i)
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

	delete[] powers;
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

		if (power >= 2 && power <= 9 && count >= 2 && count <= 50 && hiFactor >= 1)
		{
			// Ищем коэффициенты. При ошибке функция вернёт false
			if (SearchForFactors(power, count, hiFactor))
				return 0;

			// Что-то пошло не так. Сообщение об ошибке уже выведено
			return 1;
		}
	}

	// Аргументы в командной строке некорректны
	printf("Error: invalid command line\n");
	return 1;
}

} // namespace lab
