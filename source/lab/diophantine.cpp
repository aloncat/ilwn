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

#include <math.h>

namespace lab {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск примитивных решений (значений коэффициентов) Диофантова уравнения вида k.1.n. Параметр k задаёт степень уравнения,
//   единица означает, что в левой части всегда только один коэффициент, параметр n задаёт количество коэффициентов в правой
//   части уравнения. Параметры командной строки описаны в функции DiophantineMain
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
	// При нажатии Ctrl-C прервём работу, вернув true
	if (util::SystemConsole::Instance().IsCtrlCPressed())
		return true;

	static uint32_t lastTick;
	if (uint32_t tick = ::GetTickCount(); tick - lastTick >= 500)
	{
		lastTick = tick;

		auto s = util::Format("\rTesting %u=%u", factors[0], factors[1]);
		for (int i = 1, items = (count < 3) ? 1 : (count < 8) ? 2 : 3; i < items; ++i)
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
		std::sort(factors.begin(), factors.end(), std::greater());
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
				{
					return factors[i] < rhs.factors[i];
				}
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

		unsigned lowest = UINT_MAX;
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
class HashTable
{
public:
	HashTable(unsigned maxFactor, const uint64_t* powers)
	{
		m_Table = new uint32_t[16 * 1024];
		memset(m_Table, 0, 4 * 16 * 1024);

		for (unsigned i = 1; i <= maxFactor; ++i)
		{
			auto hash = GetHash(powers[i]);
			m_Table[hash >> 5] |= 1 << (hash & 31);
		}
	}

	~HashTable()
	{
		if (m_Table)
		{
			delete[] m_Table;
		}
	}

	bool Exists(uint64_t value) const
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 5] & (1 << (hash & 31));
	}

protected:
	static unsigned GetHash(uint64_t value)
	{
		// 19 младших бит
		return value & 0x7ffff;
	}

	uint32_t* m_Table = nullptr;
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
static bool SearchForFactors(int power, int count, unsigned hiFactor)
{
	constexpr int MAX_COUNT = 50;
	constexpr unsigned MAX_FACTOR = 99999;

	if (power < 1 || power > 9 || count < 2 || count > MAX_COUNT)
		return false;

	util::BinaryFile log;
	if (!OpenLogFile(log, power, count))
	{
		aux::Printf("Error: failed to open log file\n");
		return false;
	}

	UpdateConsoleTitle(power, count);
	aux::Printf("Searching for factors (%i.1.%i)\n", power, count);

	uint64_t* powers = new uint64_t[MAX_FACTOR + 1];
	const unsigned maxFactor = CalcPowers(MAX_FACTOR, power, count, powers);
	aux::Printf("Factor limit is set to #10#%u\n", maxFactor);

	// Инициализируем хеш-таблицу
	HashTable hashes(maxFactor, powers);

	// Массив k хранит коэффициенты правой части уравнения, начиная с индекса 1.
	// В элементе с индексом 0 будем хранить коэффициент левой части уравнения
	unsigned k[1 + MAX_COUNT] = { (hiFactor > 1) ? hiFactor : 2 };
	for (int i = 1; i <= count; ++i)
		k[i] = 1;

	Timer timer;
	Solutions solutions;
	bool isCancelled = false;
	bool isDone = false;

	// Основной цикл: перебираем коэффициент в левой части
	for (size_t it = 0; !isDone && k[0] <= maxFactor; ++k[0])
	{
		hiFactor = k[0];
		// Значение левой части
		const uint64_t z = powers[k[0]];
		// Пропускаем значения 1-го коэффициента в правой
		// части, при которых набор не может дать решение
		for (k[1] = 1; z > powers[k[1]] * count; ++k[1]);
		// Сумма всех членов правой части, кроме последнего
		uint64_t sum = powers[k[1]] + count - 2;

		// Внутренний цикл: перебираем коэффициенты правой части
		while (k[0] > k[1])
		{
			// Вычисляем значение степени последнего коэффициента правой части, при котором может существовать
			// решение уравнения, и проверяем значение в хеш-таблице. Если значение не будет найдено, то значит
			// не существует такого целого числа, степень которого равна lastFP и можно пропустить этот набор
			if (const uint64_t lastFP = z - sum; hashes.Exists(lastFP))
			{
				k[count] = 0;
				// Хеш был обнаружен. Теперь нужно найти число, соответствующее значению степени lastFP. Так
				// как массив степеней powers упорядочен по возрастанию, то используем бинарный поиск. Если
				// коллизии хешей не было, то мы обнаружим значение в массиве, а его индекс будет искомым
				// числом. Искомое значение не может превышать значения предпоследнего коэффициента
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
				if (k[count] && OnSolutionFound(log, solutions, power, count, k[0], k + 1))
				{
					// Если нашли 100 решений, заканчиваем работу
					if (solutions.Count() >= 100)
					{
						isDone = true;
						break;
					}
				}
			}

			// Периодически выводим текущий прогресс. Если пользователь
			// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
			if (!(++it & 0x1fffff) && UpdateProgress(count, k))
			{
				isDone = isCancelled = true;
				break;
			}

			int idx = 0;
			// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
			// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
			for (int i = count - 1;; --i)
			{
				sum -= powers[k[i]];
				if (k[i - 1] > k[i])
				{
					const auto f = ++k[i];
					if (auto n = sum + powers[f]; n < z || i == 1)
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
					const uint64_t s = sum - rem + 1;
					for (unsigned step = hi >> 1; step; step >>= 1)
					{
						auto f = k[idx] + step;
						if (hi >= f && z > s + powers[f - 1] * rem)
							k[idx] = f;
					}

					hi = k[idx++];
					sum += powers[hi] - 1;
				}
			}
		}
	}

	aux::Printf("\rTask %s, solutions found: #6#%u\n", isCancelled ?
		"cancelled" : "finished", solutions.Count());
	aux::Printf("Running time: %.2fs\n", timer.Elapsed());

	delete[] powers;

	if (hiFactor <= maxFactor)
	{
		auto s = util::Format("Highest factor: %u\n", hiFactor);
		log.Write(s.c_str(), s.size());
	}
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
