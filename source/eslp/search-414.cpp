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
	upperLimit >>= 1;

	if (m_Pow64)
		m_Hashes.Init(upperLimit, static_cast<Powers<uint64_t>&>(powers));
	else
		m_Hashes.Init(upperLimit, static_cast<Powers<UInt128>&>(powers));
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search414::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 20000) ? 2400 :
		2400 + (20000 - hiFactor) / 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 6000) ? 0xff : (f < 14000) ? 0x3f : 7;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::SelectNextTask(Task& task)
{
	++task.factors[0];
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
template<class NumberT>
AML_NOINLINE void Search414::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение последних 3 коэффициентов правой части не должно превышать значения коэффициента
	// в левой части. Но так как все они чётны, то мы будем перебирать значения, вдвое меньшие реальных
	const unsigned limit = k[0] >> 1;

	for (k[1] = 1; k[1] < k[0]; k[1] += 2)
	{
		auto left = z - powers[k[1]];
		// Разность значений левой части уравнения и степени первого коэффициента правой части всегда
		// кратна 16 (как разность биквадратов нечётных чисел). И так как другие 3 коэффициента правой
		// части чётные, то значение разности должно быть сравнимо с 0, 16, 32 или 48 модулю 256
		if ((left & 255) > 48)
			continue;

		// Если k[1] не кратен 5, то все оставшиеся 3 коэффициента правой части должны быть кратны 5.
		// В этом случае значение left должно быть кратно 5^4. Если это не так, то решений не будет
		if (k[1] % 5 && left % 625)
			continue;

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
		if (!(++worker->progressCounter & m_ProgressMask))
		{
			k[2] = (k[1] > 10) ? k[1] - k[1] % 10 : 10;
			if (OnProgress(worker, k))
				return;
		}
	}
}
