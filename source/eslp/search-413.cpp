//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-413.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search413::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 3);
	return L"#15specialized #7algorithm for #6#4.1.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::InitHashTable(PowersBase& powers, unsigned upperLimit)
{
	// NB: так как последний коэффициент правой части всегда кратен 8 и мы ищем уменьшенное в
	// 8 раз значение этого коэффициента, то имеет смысл проинициализировать лишь 1/8 значений
	// хеш-таблицы. Это значительно уменьшит количество коллизий и увеличит скорость работы

	if (m_Pow64)
		m_Hashes.Init(upperLimit >> 3, static_cast<Powers<uint64_t>&>(powers));
	else
		m_Hashes.Init(upperLimit >> 3, static_cast<Powers<UInt128>&>(powers));
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search413::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 4000000) ? 150000 :
		150000 + (4000000 - hiFactor) / 32;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	// Z должен быть сравним с 1 по модулю 8 и не кратен 5
	while ((task.factors[0] & 7) != 1 || !(task.factors[0] % 5))
		++task.factors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 320000) ? 63 : (f < 800000) ? 15 : 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::SelectNextTask(Task& task)
{
	task.factors[0] += 8;
	if (!(task.factors[0] % 5))
		task.factors[0] += 8;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void Search413::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение оставшихся коэффициентов не должно превышать значения коэффициента в левой
	// части. Но так как они оба кратны 8, то мы будем перебирать значения, уменьшенные в 8 раз
	const unsigned limit = k[0] >> 3;

	// NB: так как сумма или разность k[0] и k[1] должна быть кратна 1024, то нет смысла проверять каждое
	// нечётное значение k[1] в цикле. Из каждых 1024 натуральных чисел нам будут нужны только 2: одно,
	// которое будет в сумме с k[0] кратно 1024, и второе, которое будет кратно в разности с ним
	auto f1 = k[0] & 1023;
	f1 = (f1 < 512) ? f1 : 1024 - f1;
	const unsigned delta[] = { 1024 - 2 * f1, 2 * f1 };

	const bool zm3 = !(k[0] % 3);
	for (unsigned k1 = f1, dt = 0; k1 < k[0]; k1 += delta[dt], dt ^= 1)
	{
		auto zd = z - powers[k1];
		// Разность значений левой части уравнения и степени первого коэффициента правой части всегда кратна 4096
		// (как разность биквадратов, учитывая условие выше). И так как другие 2 коэффициента правой части кратны
		// 8, то значение разности должно быть сравнимо с 0, 4096 или 8192 модулю 65536. Кроме этого, если коэф-т
		// в левой части кратен 3, то ни один из коэффициентов правой части не может быть кратен 3
		if ((zd & 65535) > 8192 || (zm3 && !(k1 % 3)))
			continue;

		// Также разность (как и сумма степеней оставшихся коэффициентов правой части) не может
		// быть сравнима с 7, 8, 11 по модулю 13, а также 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		k[1] = k1;
		zd >>= 12;

		for (unsigned k2 = 5; k2 <= limit; k2 += 5)
		{
			k[2] = k2 << 3;
			const auto pk2 = powers[k2];
			if (pk2 >= zd)
				break;

			if (const NumberT lastFP = zd - pk2; m_Hashes.Exists(lastFP))
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
						k[3] = mid << 3;
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
		OnProgress(worker, k);
}
