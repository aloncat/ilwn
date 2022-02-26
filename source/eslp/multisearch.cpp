//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "multisearch.h"

#include "search-523.h"

#include <core/debug.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция MultiSearch::CreateInstance
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
MultiSearch::Instance MultiSearch::CreateInstance(int power, int leftCount, int rightCount)
{
	if (power == 5 && leftCount == 2 && rightCount == 3)
		return std::make_unique<Search523>();

	return std::make_unique<MultiSearch>();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   MultiSearch - универсальный алгоритм поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
bool MultiSearch::MightHaveSolution(const Task& task) const
{
	const unsigned* factors = task.factors;
	// NB: у нас нет оптимизаций для нечётных степеней
	if (m_Info.leftCount != 1 || (m_Info.power & 1))
		return true;

	// Уравнения 2.1.n
	if (m_Info.power == 2)
	{
		// Z не может быть чётным для n < 4
		if (m_Info.rightCount < 4 && !(factors[0] & 1))
			return false;
	}
	// Уравнение 4.1.n
	else if (m_Info.power == 4)
	{
		// Для n < 16
		if (m_Info.rightCount < 16)
		{
			// Z не может быть чётным
			if (!(factors[0] & 1))
				return false;
			// Z не может быть кратным 5 для n < 5
			if (m_Info.rightCount < 5 && !(factors[0] % 5))
				return false;
		}
	}
	// Уравнение 6.1.n
	else if (m_Info.power == 6)
	{
		// Для n < 8
		if (m_Info.rightCount < 8)
		{
			// Z не может быть чётным
			if (!(factors[0] & 1))
				return false;
		}
		// Для n < 9
		if (m_Info.rightCount < 9)
		{
			// Z не может быть кратно 3
			if (!(factors[0] % 3))
				return false;
			// Z не может быть кратным 7 для n < 7
			if (m_Info.rightCount < 7 && !(factors[0] % 7))
				return false;
		}
	}
	// Уравнение 8.1.n
	else if (m_Info.power == 8)
	{
		// Z не может быть чётным для n < 32
		if (m_Info.rightCount < 32 && !(factors[0] & 1))
			return false;
	}
	// Уравнение 10.1.n
	else if (m_Info.power == 10)
	{
		// Z не может быть кратным 11 для n < 11
		if (m_Info.rightCount < 11 && !(factors[0] % 11))
			return false;
	}

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
void MultiSearch::SearchFactors(Worker* worker, const NumberT* powers)
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

	// Количество "перебираемых коэффициентов"
	const int count = factorCount - worker->task.factorCount;
	// Массив k, начиная с индекса 1, созержит перебираемые коэффициенты правой части уравнения. В
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
		if (!(it++ & 0xfff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, factors))
				return;
		}

		int idx = 0;
		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
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
				// NB: для уравнений с одинаковым числом коэффициентов в левой и правой частях, мы не хотим перебирать
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
					if (hi >= f && z > s + powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx++];
				sum += powers[hi] - 1;
			}
		}
	}
}
