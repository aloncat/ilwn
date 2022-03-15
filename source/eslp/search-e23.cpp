//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-e23.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchE23::GetAdditionalInfo() const
{
	Assert(!(m_Info.power & 1) && m_Info.power <= 20);
	Assert(m_Info.leftCount == 2 && m_Info.rightCount == 3);

	return L"#15optimized #7algorithm for #6#E.2.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchE23::BeforeCompute(unsigned upperLimit)
{
	FactorSearch::BeforeCompute(upperLimit);
	SetSearchFn(this);

	m_CheckTaskFn = [](const Task& task) {
		// Коэффициенты левой части не могут быть чётными одновременно. Оптимизация одновременной
		// кратности 3 и 5 для 4-й степени не имеет смысла, т.к. даёт слабый эффект и только для
		// случая rightCount == 3. Оптимизации для 6-й степени не имеют смысла
		return (task.factors[0] & 1) || (task.factors[1] & 1);
	};
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchE23::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

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
		// Если чётность коэффициентов левой части разная (odd == 1) и k[2] нечётен, то k[3] должен быть чётным (в
		// этом случае в правой части лишь 1 нечётный коэф-т); если оба коэффициента левой части нечётны (odd == 0),
		// и k[2] чётен, то k[3] должен быть нечётным (в этом случае в правой части 2 нечётных коэф-та k[3] и k[4])
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
		if (!(it++ & 0x1ff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}
