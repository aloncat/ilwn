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
void Search414::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 2;
	task.factors[0] = startFactors[0];

	// NB: первый коэффициент в правой части должен быть кратен 10
	unsigned f = (startFactors.size() > 1) ? startFactors[1] : 10;
	task.factors[1] = std::max(10u, f - (f % 10));

	// Пропускаем чётные и кратные 5 значения коэф-та в левой части
	while (!(task.factors[0] & 1) || !(task.factors[0] % 5))
	{
		++task.factors[0];
		task.factors[1] = 10;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::SelectNextTask(Task& task)
{
	unsigned* k = task.factors;

	if (k[1] += 10; k[0] <= k[1])
	{
		++k[0];
		k[1] = 10;

		// Z не может быть чётным или кратным 5
		while (!(k[0] & 1) || !(k[0] % 5))
			++k[0];
	}
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
template<class NumberT>
AML_NOINLINE void Search414::SearchFactors(Worker* worker, const NumberT* powers)
{
	// NB: размер массива коэффициентов не может быть
	// менее 8 элементов (см. функцию OnProgress)
	unsigned k[8];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];
	// Первый коэф-т правой части
	k[1] = worker->task.factors[1];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	auto sum = powers[k[1]];
	for (unsigned k2 = 10; k2 <= k[1]; k2 += 10)
	{
		k[2] = k2;
		if (sum += powers[k2]; sum >= z)
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
						if (OnSolutionFound(worker, k))
							return;
						break;
					}
				}
			}
		}

		sum -= powers[k2];
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & 15))
	{
		if (OnProgress(worker, k))
			return;
	}
}
