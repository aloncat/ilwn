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
	Assert(m_Info.leftCount == 2 && m_Info.rightCount == 2);
	return L"#15optimized #7algorithm for #6#X.2.2";
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
	for (auto sum = powers[k[2]];;)
	{
		k[3] = 1;
		for (unsigned step = k[2] >> 1; step; step >>= 1)
		{
			auto f = k[3] + step;
			if (f <= k[2] && z > sum + powers[f - 1] * 2)
				k[3] = f;
		}

		for (sum += powers[k[3]];;)
		{
			if (const auto lastFP = z - sum; m_Hashes.Exists(lastFP))
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

			auto f = ++k[3];
			if (f > k[2])
				break;

			sum += powers[f] - powers[f - 1];
			if (sum >= z)
				break;
		}

		sum = powers[++k[2]];
		if (sum + 2 > z)
			break;

		// Вывод прогресса
		if (!(it++ & 0x7f) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}
