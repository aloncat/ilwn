//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x1x.h"

#include <core/debug.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Общие функции для коэффициента левой части уравнения
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX1XCommon::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX1XCommon::MightHaveSolution(const Task& task) const
{
	// Все чётные степени от 2 до 20 включительно
	if (!(m_Info.power & 1) && m_Info.power <= 20)
	{
		// Z не может быть чётным
		if (!(task.factors[0] & 1))
			return false;

		if (m_Info.power == 2)
		{
			if (m_Info.rightCount == 2 && !(task.factors[0] % 3))
				return false;
		}
		else if (m_Info.power == 4)
		{
			if (m_Info.rightCount == 2 && !(task.factors[0] % 3))
				return false;
			if (!(task.factors[0] % 5))
				return false;
		}
		else if (m_Info.power == 6)
		{
			if (!(task.factors[0] % 3) || !(task.factors[0] % 7))
				return false;
		}
		else if (m_Info.power == 8)
		{
			if (!(task.factors[0] % 5))
				return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX12 - улучшенный алгоритм для уравнений x.1.2
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX12::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 1 && m_Info.rightCount == 2);
	return L"#15optimized #7algorithm for #6#X.1.2";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX12::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX12::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * 2)
			k[1] = f;
	}

	for (; k[1] < k[0]; ++k[1])
	{
		if (const auto lastFP = z - powers[k[1]]; m_Hashes.Exists(lastFP))
		{
			for (unsigned lo = 1, hi = k[1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[2] = mid;
					if (OnSolutionFound(worker, k))
						return;
					break;
				}
			}
		}
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & 0x1ff))
	{
		if (OnProgress(worker, k))
			return;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX13 - улучшенный алгоритм для уравнений x.1.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX13::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 1 && m_Info.rightCount == 3);
	return L"#15optimized #7algorithm for #6#X.1.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX13::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX13::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * 3)
			k[1] = f;
	}

	// Перебор старшего коэф-та правой части
	for (size_t it = 0; k[1] < k[0]; ++k[1])
	{
		// Левая часть без старшего коэф-та
		const auto zd = z - powers[k[1]];

		k[2] = 1;
		// Пропускаем низкие значения 2-го коэф-та
		for (unsigned step = k[1] >> 1; step; step >>= 1)
		{
			auto f = k[2] + step;
			if (zd > powers[f - 1] * 2)
				k[2] = f;
		}

		// Перебор 2-го коэф-та
		for (; k[2] <= k[1]; ++k[2])
		{
			const auto pk2 = powers[k[2]];
			if (pk2 >= zd)
				break;

			if (const auto lastFP = zd - pk2; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k[2]; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[3] = mid;
						if (OnSolutionFound(worker, k))
							return;
						break;
					}
				}
			}
		}

		// Вывод прогресса
		if (!(it++ & 0x7f) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchE1X - улучшенный алгоритм для уравнений p.1.n (для чётных p при некоторых n)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchE1X::IsSuitable(int power, int leftCount, int rightCount)
{
	// Степень должна быть чётной, >= 4 и <= 20; слева 1 коэф-т
	if ((power & 1) || power < 4 || power > 20 || leftCount != 1)
		return false;

	// NB: условия ниже основаны на свойстве чётных степеней: если z не кратно 2, то z в соответствующей
	// степени будет сравнимо с 1 по модулю 8, 16, 32 или 64 (модуль зависит от показателя степени)

	if (rightCount < 8)
		return true;

	if (!(power & 3) && rightCount < 16)
		return true;

	if (!(power & 7) && rightCount < 32)
		return true;

	if (power == 16 && rightCount < 64)
		return true;

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchE1X::GetAdditionalInfo() const
{
	Assert(IsSuitable(m_Info.power, m_Info.leftCount, m_Info.rightCount));
	return L"#15optimized #7algorithm for #6#E.1.X";
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchE1X::MightHaveSolution(const Task& task) const
{
	// Z не может быть чётным
	if (!(task.factors[0] & 1))
		return false;

	// NB: для уравнений p.1.2 и p.1.3 оптимизации не нужны, так как есть отдельные
	// оптимизированные алгоритмы. Оптимизаций для 2.1.n, где n >= 4, - вообще нет

	// Уравнение 4.1.n (n < 16)
	if (m_Info.power == 4)
	{
		// Z не может быть кратным 5 для n < 5
		if (m_Info.rightCount < 5 && !(task.factors[0] % 5))
			return false;
	}
	// Уравнение 6.1.n (n < 8)
	else if (m_Info.power == 6)
	{
		// Z не может быть кратно 3
		if (!(task.factors[0] % 3))
			return false;
		// Z не может быть кратным 7 для n < 7
		if (m_Info.rightCount < 7 && !(task.factors[0] % 7))
			return false;
	}
	// Уравнение 8.1.n (n < 32)
	else if (m_Info.power == 8)
	{
		// Z не может быть кратным 5 для n < 5
		if (m_Info.rightCount < 5 && !(task.factors[0] % 5))
			return false;
	}
	// Уравнение 10.1.n (n < 8)
	else if (m_Info.power == 10)
	{
		// Z не может быть кратным 11 для n < 11
		if (!(task.factors[0] % 11))
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchE1X::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchE1X::SearchFactors(Worker* worker, const NumberT* powers)
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
	const auto z = powers[factors[0]];

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

	int oddIndex = INT_MAX;
	// Индекс левого нечётного коэффициента
	for (int i = 1; i <= m_Info.rightCount; ++i)
	{
		if (factors[i] & 1)
		{
			oddIndex = i - worker->task.factorCount + 1;
			break;
		}
	}

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
					if (OnSolutionFound(worker, factors))
						return;
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
		const unsigned delta = (count - 1 > oddIndex) ? 2 : 1;
		if (const auto f = k[count - 1]; f + delta <= k[count - 2])
		{
			k[count - 1] = f + delta;
			if (sum += powers[f + delta] - powers[f]; sum < z)
				continue;
		}

		int idx = 0;
		// Значение предпоследнего коэффициента или сумма оказались слишком большими. Переходим
		// к следующему набору коэффициентов правой части, меняя сразу несколько коэффициентов
		for (int i = count - 1;; --i)
		{
			sum -= powers[k[i]];

			if (const auto f = k[i] + ((i > oddIndex) ? 2 : 1); f <= k[i - 1])
			{
				k[i] = f;
				if (auto n = sum + powers[f]; n < z)
				{
					if (i <= oddIndex)
					{
						oddIndex = (f & 1) ? i : INT_MAX;
					}
					sum = n;
					break;
				}
				else if (i == 1)
					return;
			}
			else if (i == 1)
				return;

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
			for (int rem = count - idx + 1; rem > 1; --rem, ++idx)
			{
				const auto s = sum - (rem - 1);
				for (unsigned step = hi >> 1; step; step >>= 1)
				{
					auto f = k[idx] + step;
					if (z > s + powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx];
				if (idx > oddIndex)
				{
					k[idx] = hi += hi & 1;
					if (hi > k[idx - 1])
						k[idx] = hi -= 2;
				}
				else if (hi & 1)
					oddIndex = idx;

				sum += powers[hi] - 1;
			}
		}
	}
}
