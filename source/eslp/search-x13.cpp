//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x13.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX13::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 1 && m_Info.rightCount == 3);
	return L"#15optimized #7algorithm for #6#X.1.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX13::BeforeCompute(unsigned upperLimit)
{
	FactorSearch::BeforeCompute(upperLimit);

	SetSearchFn(this);
	m_CheckTaskFn = GetCheckTaskFn();
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX13::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

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
		if (!(it++ & 0xff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}
