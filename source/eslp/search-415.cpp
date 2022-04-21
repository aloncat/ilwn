//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-415.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool Search415::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	return power == 4 && leftCount == 1 && rightCount == 5;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search415::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 5);
	return L"#15specialized #7algorithm for #6#4.1.5";
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search415::BeforeCompute(unsigned upperLimit)
{
	SetSearchFn(this);

	// Так как последний коэффициент правой части всегда чётный и мы ищем уменьшенное вдвое
	// значение этого коэффициента, то имеет смысл проинициализировать лишь половину значений
	// хеш-таблицы. Это уменьшит количество коллизий и немного увеличит скорость работы
	InitHashTable(m_Hashes, upperLimit >> 1);

	m_CheckTaskFn = [](const WorkerTask& task) {
		// Z не может быть чётным
		return (task.factors[0] & 1) != 0;
	};
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search415::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 6000) ? 1200 :
		1200 + (6000 - hiFactor) / 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search415::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 2000) ? 1023 : (f < 8000) ? 255 : 63;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search415::SelectNextTask(WorkerTask& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void Search415::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение последних 4 коэффициентов правой части не должно превышать значения коэффициента
	// в левой части. Но так как все они чётны, то мы будем перебирать значения, вдвое меньшие реальных
	const unsigned limit = k[0] >> 1;

	const bool zm5 = !(k[0] % 5);
	// Перебор нечётного коэффициента
	for (k[1] = 1; k[1] < k[0]; k[1] += 2)
	{
		auto left = z - powers[k[1]];
		// Разность значений левой части уравнения и степени нечётного коэффициента правой части всегда
		// кратна 16 (как разность биквадратов нечётных чисел). И так как другие 4 коэффициента правой
		// части чётные, то значение разности должно быть сравнимо с 0, 16, 32, 48 или 64 модулю 256
		if ((left & 255) > 64)
			continue;

		if (zm5)
		{
			// Если коэффициент в левой части кратен 5, то ни один
			// из коэффициентов правой части не может быть кратен 5
			if (!(k[1] % 5))
				continue;
		}
		// Если коэффициент в левой части и нечётный коэффициент правой не кратны 5, то другие
		// 4 коэф-та правой части будут кратны 5, а значит и разность должна быть кратна 5^4
		else if (k[1] % 5 && left % 625)
			continue;

		left >>= 4;
		unsigned k2 = 1;
		// Пропустим слишком низкие значения k[2]
		for (unsigned step = limit >> 1; step; step >>= 1)
		{
			auto f = k2 + step;
			if (left > powers[f - 1] << 2)
				k2 = f;
		}

		// Перебор значений k[2]
		for (; k2 <= limit; ++k2)
		{
			if (zm5 && !(k2 % 5))
				continue;

			k[2] = k2 << 1;
			const auto pk2 = powers[k2];
			if (pk2 + 3 > left)
				break;

			if (SearchLast(worker, left - pk2, k, powers))
				return;

			// Вывод прогресса
			if (!(++worker->progressCounter & m_ProgressMask))
			{
				if (k[1] < k[2])
					k[2] = k[1] & ~1u;
				if (OnProgress(worker, k))
					return;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE bool Search415::SearchLast(Worker* worker, NumberT z, unsigned* k, const NumberT* powers)
{
	// На входе z содержит значение степени коэффициента левой части, из которого вычтено значение
	// степени нечётного коэффициента правой части k[1], а также значение степени старшего чётного
	// коэф-та k[2]. Коэффициенты k[2], k[3], k[4] и k[5] - это чётные коэффициенты правой части

	unsigned k3 = 1;
	// Пропустим низкие значения k[3]
	for (unsigned step = k[2] >> 2; step; step >>= 1)
	{
		auto f = k3 + step;
		if (z > powers[f - 1] * 3)
			k3 = f;
	}

	// Перебор значений коэффициента k[3]
	for (unsigned k2 = k[2] >> 1; k3 <= k2; ++k3)
	{
		k[3] = k3 << 1;
		const auto pk3 = powers[k3];
		if (pk3 >= z)
			break;

		const auto zd = z - pk3;
		// Также разность (как и сумма степеней оставшихся коэффициентов правой части) не может
		// быть сравнима с 7, 8, 11 по модулю 13, а также 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		unsigned k4 = 1;
		// Пропустим низкие значения k[4]
		for (unsigned step = k3 >> 1; step; step >>= 1)
		{
			auto f = k4 + step;
			if (zd > powers[f - 1] << 1)
				k4 = f;
		}

		// Перебор значений k[4]
		for (; k4 <= k3; ++k4)
		{
			k[4] = k4 << 1;
			const auto pk4 = powers[k4];
			if (pk4 >= zd)
				break;

			if (const NumberT lastFP = zd - pk4; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k4; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[5] = mid << 1;
						if (OnSolutionFound(worker, k))
							return true;
						break;
					}
				}
			}
		}
	}

	return false;
}
