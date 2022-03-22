//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x23.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX23::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	return leftCount == 2 && rightCount == 3;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX23::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 2 && m_Info.rightCount == 3);
	return L"#15optimized #7algorithm for #6#X.2.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX23::BeforeCompute(unsigned upperLimit)
{
	FactorSearch::BeforeCompute(upperLimit);
	SetSearchFn(this);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX23::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

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
		const auto zd = z - pk2;

		k[3] = 1;
		// Пропускаем низкие значения 2-го коэф-та
		for (unsigned step = k[2] >> 1; step; step >>= 1)
		{
			auto f = k[3] + step;
			if (zd > powers[f - 1] << 1)
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
						if (OnSolutionFound(worker, k))
							return;
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
