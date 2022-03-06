//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-414.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search414::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 4);
	return L"#15specialized #7algorithm for #6#4.1.4";
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Search414::MightHaveSolution(const Task& task) const
{
	// Z не может быть чётным или кратным 5
	if (!(task.factors[0] & 1) || !(task.factors[0] % 5))
		return false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void Search414::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	size_t it = 0;
	for (k[1] = 10; k[1] < k[0]; k[1] += 10)
	{
		for (unsigned k2 = 10; k2 < k[1]; k2 += 10)
		{
			k[2] = k2;
			const auto sum = powers[k[1]] + powers[k2];
			if (sum >= z)
				break;

			const auto zd = z - sum;
			for (unsigned k3 = 5; k3 < k[0]; k3 += 5)
			{
				k[3] = k3;
				const auto pk3 = powers[k3];
				if (pk3 >= zd)
					break;

				if (const NumberT lastFP = zd - pk3; m_Hashes.Exists(lastFP))
				{
					for (unsigned lo = 1, hi = k[0] - 1; lo <= hi;)
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

				// Вывод прогресса
				if (!(it++ & 0x7fff) && !(++worker->progressCounter & 0x7f))
				{
					if (OnProgress(worker, k))
						return;
				}
			}
		}
	}
}
