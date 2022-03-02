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
					OnSolutionFound(worker, k);
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
						OnSolutionFound(worker, k);
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
