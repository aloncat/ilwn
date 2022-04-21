//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-414.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool Search414::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	return power == 4 && leftCount == 1 && rightCount == 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search414::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 4);
	return L"#15specialized #7algorithm for #6#4.1.4";
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::BeforeCompute(unsigned upperLimit)
{
	SetSearchFn(this);

	// Так как последний коэффициент правой части всегда чётный и мы ищем уменьшенное вдвое
	// значение этого коэффициента, то имеет смысл проинициализировать лишь половину значений
	// хеш-таблицы. Это уменьшит количество коллизий и немного увеличит скорость работы
	InitHashTable(m_Hashes, upperLimit >> 1);

	m_CheckTaskFn = [](const WorkerTask& task) {
		// Z не может быть чётным или кратным 5
		return (task.factors[0] & 1) && (task.factors[0] % 5);
	};
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search414::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 24000) ? 2800 :
		2800 + (24000 - hiFactor) / 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 8000) ? 511 : (f < 16000) ? 63 : 15;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::SelectNextTask(WorkerTask& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void Search414::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение последних 3 коэффициентов правой части не должно превышать значения коэффициента
	// в левой части. Но так как все они чётны, то мы будем перебирать значения, вдвое меньшие реальных
	const unsigned limit = k[0] >> 1;

	for (k[1] = 1; k[1] < k[0]; k[1] += 2)
	{
		auto left = z - powers[k[1]];
		// Разность значений левой части уравнения и степени нечётного коэффициента правой части всегда
		// кратна 16 (как разность биквадратов нечётных чисел). И так как другие 3 коэффициента правой
		// части чётные, то значение разности должно быть сравнимо с 0, 16, 32 или 48 модулю 256
		if ((left & 255) > 48)
			continue;

		// Если k[1] не кратен 5, то все оставшиеся 3 коэффициента правой части должны быть кратны 5.
		// В этом случае значение left должно быть кратно 5^4. Если это не так, то решений не будет
		if (k[1] % 5 && left % 625)
			continue;

		left >>= 4;
		unsigned k2 = 1;
		// Пропустим слишком низкие значения k[2]
		for (unsigned step = limit >> 1; step; step >>= 1)
		{
			auto f = k2 + step;
			if (left > powers[f - 1] * 3)
				k2 = f;
		}

		const unsigned k2dt = (k[1] % 5) ? 5 : 1;
		for (k2 += (k2dt == 1) ? 0 : 4 - (k2 + 4) % 5; k2 <= limit; k2 += k2dt)
		{
			k[2] = k2 << 1;
			const auto pk2 = powers[k2];
			if (pk2 >= left)
				break;

			const auto zd = left - pk2;
			// Также разность (как и сумма степеней оставшихся коэффициентов правой части) не может
			// быть сравнима с 7, 8, 11 по модулю 13, а также 4, 5, 6, 9, 13, 22, 28 по модулю 29
			if ((0x980 & (1 << (zd % 13))) || (0x10402270 & (1 << (zd % 29))))
				continue;

			unsigned k3 = 1;
			// Пропустим слишком низкие значения k[3]
			for (unsigned step = k2 >> 1; step; step >>= 1)
			{
				auto f = k3 + step;
				if (zd > powers[f - 1] << 1)
					k3 = f;
			}

			const unsigned k3dt = (k2dt != 1 || k2 % 5) ? 5 : 1;
			for (k3 += (k3dt == 1) ? 0 : 4 - (k3 + 4) % 5; k3 <= k2; k3 += k3dt)
			{
				k[3] = k3 << 1;
				const auto pk3 = powers[k3];
				if (pk3 >= zd)
					break;

				if (const NumberT lastFP = zd - pk3; m_Hashes.Exists(lastFP))
				{
					for (unsigned lo = 1, hi = k3; lo <= hi;)
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
			k[2] = k[1] - ((k[1] > 10) ? k[1] % 10 : 0);
			if (OnProgress(worker, k))
				return;
		}
	}
}
