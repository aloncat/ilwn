//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "multisearch.h"

#include "search-414.h"
#include "search-x1x.h"
#include "search-x2x.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция MultiSearch::CreateInstance
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
MultiSearch::Instance MultiSearch::CreateInstance(int power, int leftCount, int rightCount, const Options& options)
{
	if (!options.HasOption("nocustom"))
	{
		if (leftCount == 1)
		{
			if (power == 4 && rightCount == 4)
				return std::make_unique<Search414>();

			if (rightCount == 2)
				return std::make_unique<SearchX12>();
			if (rightCount == 3)
				return std::make_unique<SearchX13>();
			if (SearchE1X::IsSuitable(power, leftCount, rightCount))
				return std::make_unique<SearchE1X>();
		}
		else if (leftCount == 2)
		{
			if (rightCount == 2)
				return std::make_unique<SearchX22>();

			if (rightCount == 3)
			{
				// X23 и E23 - оптимизированные алгоритмы для уравнения вида x.2.3. X23 подходит для любых степеней;
				// но E23 имеет дополнительные оптимизации и подходит только для чётных степеней до 20-й включительно
				return (power & 1) ? std::make_unique<SearchX23>() :
					std::make_unique<SearchE23>();
			}
		}
	}

	// Дефолтный универсальный алгоритм
	return std::make_unique<MultiSearch>();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   MultiSearch - универсальный алгоритм поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
void MultiSearch::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	// Для универсального алгоритма все коэффициенты
	// левой части уравнения всегда заданы заданием
	task.factorCount = m_Info.leftCount;

	if (m_Info.power > 5 && m_Info.leftCount == 1)
	{
		const int p = m_Info.power;
		const int c = m_Info.rightCount - m_Info.leftCount;
		// Дополнительно задаваемые коэффициенты правой части
		const float predefined = .08f * (p + 2) + .2f * (c + 1) - .4f;

		static const int maxCount[11] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 };
		const int maxPredefined = (m_Info.power <= 10) ? maxCount[m_Info.power] : 1;
		task.factorCount += util::Clamp(static_cast<int>(predefined), 0, maxPredefined);
	}

	const int count = std::min(task.factorCount, static_cast<int>(startFactors.size()));

	for (size_t i = 0; i < count; ++i)
		task.factors[i] = startFactors[i];

	for (int i = count; i < task.factorCount; ++i)
		task.factors[i] = 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
void MultiSearch::SelectNextTask(Task& task)
{
	unsigned* k = task.factors;

	if (task.factorCount <= m_Info.leftCount)
	{
		for (int i = task.factorCount - 1; i; --i)
		{
			if (k[i - 1] > k[i])
			{
				++k[i];
				return;
			}

			k[i] = 1;
		}

		++k[0];
		return;
	}

	for (;;)
	{
		int idx = 0;
		for (int i = task.factorCount - 1;; --i)
		{
			if (!i)
			{
				++k[0];
				break;
			}

			if (k[i - 1] > k[i])
			{
				++k[i];
				break;
			}

			k[i] = 1;
			idx = i;
		}

		if (!idx || (m_Pow64 ? SkipLowSet(task, m_Pow64) : SkipLowSet(task, m_Powers)))
			break;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
bool MultiSearch::SkipLowSet(Task& task, const NumberT* powers) const
{
	unsigned* k = task.factors;
	const auto z = powers[k[0]];
	const int rem = m_Info.rightCount;

	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * rem)
			k[1] = f;
	}

	return k[0] > k[1];
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MultiSearch::MightHaveSolution(const Task& task) const
{
	// NB: оптимизация (фильтр заданий) для универсального алгоритма ограничена
	// только чётными степенями уравнения и 1 коэффициентом в его левой части
	if (m_Info.leftCount != 1 || (m_Info.power & 1))
		return true;

	// Уравнение 2.1.n
	if (m_Info.power == 2)
	{
		// Z не может быть чётным для n < 4
		if (m_Info.rightCount < 4 && !(task.factors[0] & 1))
			return false;
	}
	// Уравнение 4.1.n
	else if (m_Info.power == 4)
	{
		// Для n < 16
		if (m_Info.rightCount < 16)
		{
			// Z не может быть чётным
			if (!(task.factors[0] & 1))
				return false;
			// Z не может быть кратным 5 для n < 5
			if (m_Info.rightCount < 5 && !(task.factors[0] % 5))
				return false;
		}
	}
	// Уравнение 6.1.n
	else if (m_Info.power == 6)
	{
		// Z не может быть чётным для n < 8
		if (m_Info.rightCount < 8 && !(task.factors[0] & 1))
			return false;

		// Для n < 9
		if (m_Info.rightCount < 9)
		{
			// Z не может быть кратно 3
			if (!(task.factors[0] % 3))
				return false;
			// Z не может быть кратным 7 для n < 7
			if (m_Info.rightCount < 7 && !(task.factors[0] % 7))
				return false;
		}
	}
	// Уравнение 8.1.n
	else if (m_Info.power == 8)
	{
		// Для n < 32
		if (m_Info.rightCount < 32)
		{
			// Z не может быть чётным
			if (!(task.factors[0] & 1))
				return false;
			// Z не может быть кратным 5 для n < 5
			if (m_Info.rightCount < 5 && !(task.factors[0] % 5))
				return false;
		}
	}
	// Степени 10, 12, ..., 20
	else if (m_Info.power <= 20)
	{
		// Z не может быть чётным...
		if (!(task.factors[0] & 1))
		{
			// ...если n < 8
			if (m_Info.rightCount < 8)
				return false;
			// ...если n < 16 (для степеней, кратных 4)
			if (!(m_Info.power & 3) && m_Info.rightCount < 16)
				return false;
			// ...если n < 64 (для 16-й степени)
			if (m_Info.power == 16 && m_Info.rightCount < 64)
				return false;
		}

		// Уравнение 10.1.n
		if (m_Info.power == 10)
		{
			// Z не может быть кратным 11 для n < 11
			if (m_Info.rightCount < 11 && !(task.factors[0] % 11))
				return false;
		}
	}

	// TODO: добавить оптимизацию левой части для x.2.x (и x.2.4 в частности) для случая чётных степеней:
	// оба коэффициента в левой части не могут быть чётными одновременно для многих значений rightCount

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void MultiSearch::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void MultiSearch::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Коэффициенты уравнения
	unsigned factors[MAX_FACTOR_COUNT];

	// Копируем коэффициенты из задания
	for (int i = 0; i < worker->task.factorCount; ++i)
		factors[i] = worker->task.factors[i];

	// Коэффициенты, не заданные заданием, инициализируем в 1
	const int factorCount = m_Info.leftCount + m_Info.rightCount;
	for (int i = worker->task.factorCount; i < factorCount; ++i)
		factors[i] = 1;

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	NumberT sum = 0;
	// Сумма правой части без последнего коэффициента
	for (int i = m_Info.leftCount; i < factorCount - 1; ++i)
		sum += powers[factors[i]];

	// Количество "перебираемых" коэффициентов
	const int count = factorCount - worker->task.factorCount;
	// Массив k, начиная с индекса 1, содержит перебираемые коэффициенты правой части уравнения. В
	// элементе с индексом 0 хранится предшествующий им коэффициент левой или правой части уравнения
	unsigned* k = factors + (worker->task.factorCount - 1);

	sum -= count - 1;
	// Пропустим часть значений 1-го перебираемого коэффициента
	// правой части, при которых набор не будет давать решение
	for (; z > sum + powers[k[1]] * count; ++k[1]);
	sum += powers[k[1]] + count - 2;

	if (sum >= z)
		return;

	// Перебираем коэффициенты правой части
	for (size_t it = 0;;)
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
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[count] = mid;
					OnSolutionFound(worker, factors);
					break;
				}
			}
		}

		// Периодически выводим текущий прогресс. Если пользователь
		// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
		if (!(it++ & 0x7ff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, factors))
				return;
		}

		// Увеличиваем предпоследний коэффициент в правой части
		if (auto f = k[count - 1]; f < k[count - 2])
		{
			if (auto n = sum - powers[f] + powers[f + 1]; n < z)
			{
				k[count - 1] = f + 1;
				sum = n;
				continue;
			}
		}

		int idx = 0;
		// Значение предпоследнего коэффициента или сумма оказались слишком большими. Переходим
		// к следующему набору коэффициентов правой части, меняя сразу несколько коэффициентов
		for (int i = count - 1;; --i)
		{
			sum -= powers[k[i]];
			if (k[i - 1] > k[i] || i == 1)
			{
				const auto f = ++k[i];
				if (auto n = sum + powers[f]; n < z)
				{
					sum = n;
					break;
				}
				else if (i == 1)
					return;
			}
			sum += k[i] = 1;
			idx = i;
		}

		if (idx)
		{
			if (idx == 2)
			{
				// Для уравнений с одинаковым числом коэффициентов в левой и правой частях мы не хотим перебирать
				// старшие коэффициенты правой части, значения которых >= значению старшего коэффициента левой части
				if (k[1] >= factors[0] && m_Info.leftCount == m_Info.rightCount)
					return;

				// Если в правой части уравнения хотя бы 1 коэффициент задан
				// заданием, то значение k[1] не должно превышать значения k[0]
				if (k[1] > k[0] && worker->task.factorCount > m_Info.leftCount)
					return;
			}

			// Каждый раз, когда мы сбрасываем в 1 коэффициент в правой части, увеличивая на 1 коэффициент слева от него,
			// переменная idx будет содержать индекс самого левого единичного коэффициента. Единичное и многие следующие
			// значения коэффициента не смогут дать решений, так как для них сумма в правой части будет меньше значения
			// левой. Поэтому мы будем пропускать такие значения, сразу переходя к тем, которые могут дать решение
			unsigned hi = k[idx - 1];
			for (int rem = count - idx + 1; rem > 1; --rem)
			{
				const auto s = sum - (rem - 1);
				for (unsigned step = hi >> 1; step; step >>= 1)
				{
					auto f = k[idx] + step;
					if (z > s + powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx++];
				sum += powers[hi] - 1;
			}
		}
	}
}
