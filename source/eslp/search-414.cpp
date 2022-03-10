//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-414.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search414::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 4);
	return L"#15specialized #7algorithm for #6#4.1.4";
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::InitHashTable(PowersBase& powers, unsigned upperLimit)
{
	// NB: так как последний коэффициент правой части всегда чётный и мы ищем уменьшенное вдвое
	// значение этого коэффициента, то имеет смысл проинициализировать лишь половину значений
	// хеш-таблицы. Это уменьшит количество коллизий и увеличит скорость работы

	if (m_Pow64)
		m_Hashes.Init(upperLimit >> 1, static_cast<Powers<uint64_t>&>(powers));
	else
		m_Hashes.Init(upperLimit >> 1, static_cast<Powers<UInt128>&>(powers));
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 2;
	task.factors[0] = startFactors[0];
	// NB: первый коэффициент в правой части должен быть нечётным
	task.factors[1] = (startFactors.size() > 1) ? startFactors[1] | 1 : 1;

	// Пропускаем чётные и кратные 5 значения коэффициента в левой части, а также те наборы,
	// в которых первый (заданный) коэффициент правой части больше коэффициента в левой части
	while (!(task.factors[0] & 1) || !(task.factors[0] % 5) || task.factors[0] <= task.factors[1])
	{
		++task.factors[0];
		task.factors[1] = 1;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::SelectNextTask(Task& task)
{
	unsigned* k = task.factors;

	if (k[1] += 2; k[0] <= k[1])
	{
		++k[0];
		k[1] = 1;

		// Z не может быть чётным или кратным 5
		while (!(k[0] & 1) || !(k[0] % 5))
			++k[0];
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search414::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 22000) ? 1200 :
		1200 + (22000 - hiFactor) / 8;
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
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];
	// Первый коэф-т правой части
	k[1] = worker->task.factors[1];

	// Разность значений левой части уравнения и степени первого коэффициента правой части всегда
	// кратна 16 (как разность биквадратов нечётных чисел). И так как другие 3 коэффициента правой
	// части чётные, то значение разности должно быть сравнимо с 0, 16, 32 или 48 модулю 256
	auto left = powers[k[0]] - powers[k[1]];
	if ((left & 255) > 48)
		return;

	// Макс. значение оставшихся коэффициентов не должно превышать значения коэффициента в левой
	// части. Но так как все они чётны, то мы будем перебирать значения, вдвое меньшие реальных
	const unsigned limit = k[0] >> 1;
	left >>= 4;

	for (unsigned k2 = 5; k2 <= limit; k2 += 5)
	{
		k[2] = k2 << 1;
		const auto pk2 = powers[k2];
		if (pk2 >= left)
			break;

		const auto zd = left - pk2;
		for (unsigned k3 = 5; k3 <= k2; k3 += 5)
		{
			k[3] = k3 << 1;
			const auto pk3 = powers[k3];
			if (pk3 >= zd)
				break;

			if (const NumberT lastFP = zd - pk3; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = limit; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[4] = mid << 1;
						if (OnSolutionFound(worker, k))
							return;
						break;
					}
				}
			}
		}
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & 31))
	{
		k[2] = k[1] - k[1] % 10;
		OnProgress(worker, k);
	}
}
