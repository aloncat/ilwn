//∙ESLP/iLWN
// Copyright (C) 2022-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include "uint128.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/file.h>
#include <core/strformat.h>
#include <core/util.h>
#include <core/winapi.h>

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
static bool OpenLogFile(util::File& file, int power, int count)
{
	if (file.Open(util::Format(L"log-%i.1.%i.txt", power, count),
		util::FILE_OPEN_ALWAYS | util::FILE_OPEN_READWRITE))
	{
		if (auto fileSize = file.GetSize(); fileSize > 0)
		{
			if (file.SetPosition(fileSize) && file.Write("---\n", 4))
				return true;
		}
		else if (fileSize == 0)
			return true;

		file.Close();
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void UpdateConsoleTitle(int power, int count, size_t solutionsFound = 0)
{
	util::SystemConsole::Instance().SetTitle(util::Format("Searching for factors (%i.1.%i):"
		" %u solutions found", power, count, solutionsFound));
}

//--------------------------------------------------------------------------------------------------------------------------------
static bool UpdateProgress(int count, const unsigned* factors)
{
	if (util::SystemConsole::Instance().IsCtrlCPressed())
		return true;

	static uint32_t lastTick;
	if (uint32_t tick = ::GetTickCount(); tick - lastTick >= 500)
	{
		lastTick = tick;

		const int desiredCount = (count < 4) ? 1 : 2 + count / 8;
		auto s = util::Format("\rTesting %u=%u", factors[0], factors[1]);
		for (int i = 1; i < desiredCount; ++i)
		{
			s += '+';
			s += std::to_string(factors[i + 1]);
		}
		s += "+...    \b\b\b\b";
		aux::Print(s);
	}

	return false;
}

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

	void Clear()
	{
		m_Solutions.clear();
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
	memset(powers, 0, sizeof(powers[0]) * (high + 1));

	high = std::min(high, ~0u / count);
	for (unsigned i = 1; i <= high; ++i)
	{
		uint64_t o, p = 1;
		const unsigned c = i * count;
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
class HashTable
{
public:
	~HashTable()
	{
		AML_SAFE_DELETEA(m_Table);
	}

	void Init(unsigned maxFactor, const uint64_t* powers)
	{
		AML_SAFE_DELETEA(m_Table);
		m_Table = new uint8_t[32 * 1024];
		memset(m_Table, 0, 32 * 1024);

		for (unsigned i = 1; i <= maxFactor; ++i)
		{
			auto hash = GetHash(powers[i]);
			m_Table[hash >> 3] |= 1 << (hash & 7);
		}
	}

	bool Exists(uint64_t value) const
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 3] & (1 << (hash & 7));
	}

protected:
	static unsigned GetHash(uint64_t value)
	{
		// 18 младших бит
		return value & 0x3ffff;
	}

	uint8_t* m_Table = nullptr;
};

//--------------------------------------------------------------------------------------------------------------------------------
static bool OnSolutionFound(util::File& log, Solutions& solutions,
	int power, int count, unsigned left, const unsigned* right)
{
	// Формируем решение и пытаемся его добавить в набор. Если
	// решение непримитивное, то функция Insert вернёт false
	if (solutions.Insert(Solution(left, right, count)))
	{
		// Формируем строку с коэффициентами
		auto s = std::to_string(left) + '=';

		for (int i = 0, k; i < count; i += k)
		{
			k = 1;
			const unsigned f = right[i];
			while (i + k < count && f == right[i + k])
				++k;

			if (i)
			{
				s += '+';
			}
			if (k > 1)
			{
				s += std::to_string(k);
				s += '*';
			}
			s += std::to_string(f);
		}

		s += "\n";
		// Выводим строку с решением на экран и в файл
		aux::Printf("\rSolution found: %s", s.c_str());
		log.Write(s.c_str(), s.size());

		// Обновляем строку заголовка консольного окна
		UpdateConsoleTitle(power, count, solutions.Count());

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
class FactorSearch
{
public:
	~FactorSearch();

	bool DoSearch(int power, int count, unsigned hiFactor);

protected:
	static constexpr int MAX_COUNT = 50;
	static constexpr unsigned MAX_FACTOR = 99999;

	void SearchFactors(unsigned k0, const std::function<bool(const unsigned*)>& progressCb);
	void OnSolutionFound(unsigned left, const unsigned* right);

protected:
	uint64_t* m_Powers = nullptr;
	HashTable m_Hashes;

	Solutions m_Solutions;
	util::BinaryFile m_Log;

	int m_Power = 0;
	int m_Count = 0;
	bool m_IsFinished = false;
};

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::~FactorSearch()
{
	AML_SAFE_DELETEA(m_Powers);
	m_Log.Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::DoSearch(int power, int count, unsigned hiFactor)
{
	if (power < 1 || power > 9 || count < 2 || count > MAX_COUNT)
		return false;

	if (!OpenLogFile(m_Log, power, count))
	{
		aux::Printf("#12Error: failed to open log file\n");
		return false;
	}

	m_Power = power;
	m_Count = count;
	m_Solutions.Clear();

	UpdateConsoleTitle(power, count);
	aux::Printf("Searching for factors (%i.1.%i equation)\n", power, count);

	m_Powers = new uint64_t[MAX_FACTOR + 1];
	const unsigned maxFactor = CalcPowers(MAX_FACTOR, power, count, m_Powers);
	aux::Printf("Factor limit is set to #10#%u #8(64 bits)\n", maxFactor);

	// Инициализируем хеш-таблицу
	m_Hashes.Init(maxFactor, m_Powers);

	Timer timer;
	m_IsFinished = false;
	bool isCancelled = false;

	// Перебираем коэффициент в левой части
	hiFactor = (hiFactor > 2) ? hiFactor : 2;
	for (unsigned k0 = hiFactor; k0 <= maxFactor && !m_IsFinished; ++k0)
	{
		hiFactor = k0;
		SearchFactors(k0, [&](const unsigned* factors) {
			if (UpdateProgress(count, factors))
			{
				m_IsFinished = true;
				isCancelled = true;
			}
			return m_IsFinished;
		});
	}

	aux::Printf("\rTask %s#7, solutions found: #6#%u\n", isCancelled ?
		"#12cancelled" : "finished", m_Solutions.Count());

	if (hiFactor <= maxFactor)
	{
		auto s = util::Format("hiFactor: %u\n", hiFactor);
		m_Log.Write(s.c_str(), s.size());

		if (isCancelled)
		{
			aux::Printf("Factor (current value): #6#%u\n", hiFactor);
		}
	}

	const float timeElapsed = timer.Elapsed();
	aux::Printf((timeElapsed < 90) ? "Running time: %.2f#8s\n" : "Running time: %.0f#8s\n", timeElapsed);

	AML_SAFE_DELETEA(m_Powers);
	m_Log.Close();

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::SearchFactors(unsigned k0, const std::function<bool(const unsigned*)>& progressCb)
{
	if (k0 < 2)
		return;

	// Массив k хранит коэффициенты правой части уравнения, начиная с индекса 1.
	// В элементе с индексом 0 будем хранить коэффициент левой части уравнения
	unsigned k[1 + MAX_COUNT] = { k0 };
	for (int i = 1; i <= m_Count; ++i)
		k[i] = 1;

	// Значение левой части
	const uint64_t z = m_Powers[k0];
	// Пропускаем значения 1-го коэффициента в правой
	// части, при которых набор не может дать решение
	for (k[1] = 1; z > m_Powers[k[1]] * m_Count; ++k[1]);
	// Сумма всех членов правой части, кроме последнего
	uint64_t sum = m_Powers[k[1]] + m_Count - 2;

	const int count = m_Count;
	// Перебираем коэффициенты правой части
	for (size_t it = 0; k[0] > k[1];)
	{
		// Вычисляем значение степени последнего коэффициента правой части, при котором может существовать
		// решение уравнения, и проверяем значение в хеш-таблице. Если значение не будет найдено, то значит
		// не существует такого целого числа, степень которого равна lastFP и можно пропустить этот набор
		if (const auto lastFP = z - sum; m_Hashes.Exists(lastFP))
		{
			// Хеш был обнаружен. Теперь нужно найти число, соответствующее значению степени lastFP. Так
			// как массив степеней powers упорядочен по возрастанию, то используем бинарный поиск. Если
			// коллизии хешей не было, то мы обнаружим значение в массиве, а его индекс будет искомым
			// числом. Искомое значение не может превышать значения предпоследнего коэффициента
			for (unsigned lo = 1, hi = k[count - 1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = m_Powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[count] = mid;
					OnSolutionFound(k[0], k + 1);
					break;
				}
			}
		}

		// Периодически выводим текущий прогресс. Если пользователь
		// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
		if (!(++it & 0xfffff) && progressCb && progressCb(k))
			break;

		int idx = 0;
		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
		for (int i = count - 1;; --i)
		{
			sum -= m_Powers[k[i]];
			if (k[i - 1] > k[i])
			{
				const auto f = ++k[i];
				if (auto n = sum + m_Powers[f]; n < z || i == 1)
				{
					sum = n;
					break;
				}
			}
			sum += k[i] = 1;
			idx = i;
		}

		if (idx)
		{
			// Каждый раз, когда мы сбрасываем в 1 коэффициент в правой части, увеличивая на 1 коэффициент слева от него,
			// переменная idx будет содержать индекс самого левого единичного коэффициента. Единичное и многие следующие
			// значения коэффициента не смогут дать решений, так как для них сумма в правой части будет меньше значения
			// левой. Поэтому мы будем пропускать такие значения, сразу переходя к тем, которые могут дать решение
			unsigned hi = k[idx - 1];
			for (int rem = count - idx + 1; rem > 1; --rem)
			{
				const uint64_t s = sum - (rem - 1);
				for (unsigned step = hi >> 1; step; step >>= 1)
				{
					auto f = k[idx] + step;
					if (hi >= f && z > s + m_Powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx++];
				sum += m_Powers[hi] - 1;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnSolutionFound(unsigned left, const unsigned* right)
{
	if (::OnSolutionFound(m_Log, m_Solutions, m_Power, m_Count, left, right))
	{
		if (m_Solutions.Count() >= 100)
			m_IsFinished = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchForFactors(int power, int count, unsigned hiFactor)
{
	FactorSearch search;
	return search.DoSearch(power, count, hiFactor);
}
