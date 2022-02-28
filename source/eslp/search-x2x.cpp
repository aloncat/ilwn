//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x2x.h"

#include <core/debug.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX22 - улучшенный алгоритм для уравнений x.2.2
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX22::GetAdditionalInfo() const
{
	Assert(m_Info.power <= 20 && m_Info.leftCount == 2 && m_Info.rightCount == 2);
	return L"#15optimized #7algorithm for #6#X.2.2";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	// NB: для x.2.2 задание задаёт только старший коэффициент левой части. 2-й коэффициент
	// в левой части, так же, как и коэффициенты в правой, перебираются в SearchFactors
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX22::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Старший коэф-т левой части
	k[0] = worker->task.factors[0];

	// NB: если значение k[0] чётно, то k[1] не может быть чётным (для любых чётных степеней), иначе
	// решение не будет примитивным. В этих случаях мы должны проверять только нечётные значения k[1]
	const unsigned delta = ((m_Info.power & 1) || (k[0] & 1)) ? 1 : 2;

	// Перебор 2-го коэф-та левой части
	for (k[1] = 1; k[1] <= k[0]; k[1] += delta)
	{
		// Сумма в левой части уравнения
		const auto z = powers[k[0]] + powers[k[1]];

		k[2] = 1;
		// Пропускаем низкие значения старшего коэф-та
		for (unsigned step = k[0] >> 1; step; step >>= 1)
		{
			auto f = k[2] + step;
			if (z > powers[f - 1] * 2)
				k[2] = f;
		}

		for (; k[2] < k[0]; ++k[2])
		{
			if (const auto lastFP = z - powers[k[2]]; m_Hashes.Exists(lastFP))
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
						OnSolutionFound(worker, k);
						break;
					}
				}
			}
		}

		// Вывод прогресса
		if (!(++worker->progressCounter & 0x3ff))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX23 - улучшенный алгоритм для уравнений x.2.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX23::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 2 && m_Info.rightCount == 3);
	return L"#15optimized #7algorithm for #6#X.2.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX23::MightHaveSolution(const Task& task) const
{
	// Все чётные степени от 2 до 20 включительно
	if (!(m_Info.power & 1) && m_Info.power <= 20)
	{
		// Коэффициенты не могут быть чётными одновременно
		if (!(task.factors[0] & 1) && !(task.factors[1] & 1))
			return false;

		// NB: оптимизация одновременной кратности 3 и 5 для 4-й степени не имеет смысла, так как даёт очень
		// небольшой эффект и только для случая rightCount == 3. Оптимизации для 6-й степени не имеют смысла
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX23::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX23::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициенты левой части
	k[0] = worker->task.factors[0];
	k[1] = worker->task.factors[1];

	// Сумма в левой части уравнения
	const auto z = powers[k[0]] + powers[k[1]];

	k[2] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[2] + step;
		if (z > powers[f - 1] * 3)
			k[2] = f;
	}

	size_t it = 0;
	// Перебор старшего коэф-та правой части
	for (auto pk2 = powers[k[2]];;)
	{
		// Левая часть без старшего коэф-та
		const auto zd = z - pk2;

		k[3] = 1;
		// Пропускаем низкие значения 2-го коэф-та
		for (unsigned step = k[2] >> 1; step; step >>= 1)
		{
			auto f = k[3] + step;
			if (zd > powers[f - 1] * 2)
				k[3] = f;
		}

		// Перебор 2-го коэф-та
		for (; k[3] <= k[2]; ++k[3])
		{
			const auto pk3 = powers[k[3]];
			if (pk3 >= zd)
				break;

			if (const auto lastFP = zd - pk3; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k[3]; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[4] = mid;
						OnSolutionFound(worker, k);
						break;
					}
				}
			}
		}

		pk2 = powers[++k[2]];
		if (pk2 + 2 > z)
			break;

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
//   SearchE23 - улучшенный алгоритм для уравнений p.2.3 (для чётных степеней p)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchE23::GetAdditionalInfo() const
{
	Assert(!(m_Info.power & 1) && m_Info.power <= 20);
	return SearchX23::GetAdditionalInfo();
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchE23::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchE23::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициенты левой части
	k[0] = worker->task.factors[0];
	k[1] = worker->task.factors[1];

	// Чётность коэф-тов (1 если разная)
	const unsigned odd = (k[0] ^ k[1]) & 1;

	// Сумма в левой части уравнения
	const auto z = powers[k[0]] + powers[k[1]];

	k[2] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[2] + step;
		if (z > powers[f - 1] * 3)
			k[2] = f;
	}

	size_t it = 0;
	// Перебор старшего коэф-та правой части
	for (auto pk2 = powers[k[2]];;)
	{
		// Левая часть без старшего коэф-та
		const auto zd = z - pk2;

		k[3] = 1;
		// Пропускаем низкие значения 2-го коэф-та
		for (unsigned step = k[2] >> 1; step; step >>= 1)
		{
			auto f = k[3] + step;
			if (zd > powers[f - 1] * 2)
				k[3] = f;
		}

		unsigned delta = 1;
		// NB: если чётность коэффициентов левой части разная (odd == 1) и k[2] нечётен, то k[3] должен быть чётным (в
		// этом случае в правой части лишь 1 нечётный коэф-т); если оба коэффициента левой части нечётны (odd == 0), и
		// k[2] чётен, то k[3] должен быть нечётным (в этом случае в правой части 2 нечётных коэф-та k[3] и k[4])
		if ((k[2] & 1) == odd)
		{
			k[3] += ((k[3] & 1) == odd) ? 1 : 0;
			delta = 2;
		}

		// Перебор 2-го коэффициента
		for (; k[3] <= k[2]; k[3] += delta)
		{
			const auto pk3 = powers[k[3]];
			if (pk3 >= zd)
				break;

			if (const auto lastFP = zd - pk3; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k[3]; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[4] = mid;
						OnSolutionFound(worker, k);
						break;
					}
				}
			}
		}

		pk2 = powers[++k[2]];
		if (pk2 + 2 > z)
			break;

		// Вывод прогресса
		if (!(it++ & 0x7f) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}
