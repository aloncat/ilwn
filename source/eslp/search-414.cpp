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

	// Так как последний коэффициент правой части всегда кратен 5 и мы ищем уменьшенное в пять
	// раз значение этого коэффициента, то имеет смысл проинициализировать лишь 1/5 значений
	// хеш-таблицы. Это уменьшит количество коллизий и немного увеличит скорость работы
	InitHashTable(m_Hashes, upperLimit / 5);

	m_CheckTaskFn = [](const WorkerTask& task) {
		// Z не может быть чётным или кратным 5
		return (task.factors[0] & 1) && task.factors[0] % 5;
	};
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search414::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 80000) ? 16000 :
		16000 + (80000 - hiFactor) / 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search414::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 36000) ? 1023 : (f < 64000) ? 255 :
		(f < 96000) ? 63 : 15;
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
	static_assert(ProgressManager::MAX_COEFS >= 5,
		"Array k is too short for 4.1.4 equation");

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение последних 3 коэффициентов правой части не должно превышать значения коэф-та в левой
	// части. Но так как все эти 3 коэф-та кратны 5, то мы будем перебирать значения, уменьшенные в 5 раз
	const unsigned limit = k[0] / 5;

	k[1] = 1;
	// Перебор коэффициента k[1], некратного 5
	for (unsigned nk1 = 5; k[1] < k[0]; ++k[1])
	{
		if (k[1] == nk1)
		{
			nk1 += 5;
			continue;
		}

		auto left = z - powers[k[1]];
		// Так как все 3 оставшиеся коэффициента правой части кратны 5, то их сумма кратна
		// 5^4, а значит и разность тоже должна быть кратна 5^4, иначе решений не будет
		if (left % 625)
			continue;

		if (k[1] & 1)
		{
			// Если значение k[1] нечётно, то разность значений левой части уравнения и степени коэффициента k[1]
			// всегда кратна 16 (как разность биквадратов нечётных чисел). И так как остальные три коэф-та правой
			// части чётные, то значение этой разности должно быть сравнимо с 0, 16, 32 или 48 модулю 256
			if ((left & 255) > 48)
				continue;

			if (SearchLast1(worker, k, limit >> 1, (left / 625) >> 4, powers))
				return;
		} else
		{
			// Значение k[1] - чётное
			if (SearchLast2(worker, k, limit, left / 625, powers))
				return;
		}

		// Вывод прогресса
		if (!(++worker->progressCounter & m_ProgressMask))
		{
			k[2] = (nk1 > 5) ? nk1 - 5 : k[1];
			if (OnProgress(worker, k))
				return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE bool Search414::SearchLast1(Worker* worker, unsigned* k, unsigned limit, NumberT z, const NumberT* powers)
{
	// Переменная z содержит значение степени нечётного коэффициента левой части, из которого вычтено
	// значение степени нечётного коэффициента правой части k[1], и делённое на 16 и 5^4. Коэффициенты
	// k[0] и k[1] нечётны и некратны 5, тогда как оставшиеся коэффициенты k[2], k[3] и k[4] кратны 10

	unsigned k2 = 1;
	// Пропустим низкие значения k[2]
	for (unsigned step = limit >> 1; step; step >>= 1)
	{
		auto f = k2 + step;
		if (z > powers[f - 1] * 3)
			k2 = f;
	}

	// Перебор значений k[2]
	for (; k2 <= limit; ++k2)
	{
		const auto pk2 = powers[k2];
		if (pk2 >= z)
			break;

		const auto zd = z - pk2;
		// Значение разности (также, как и суммы степеней оставшихся двух
		// коэффициентов) должно быть сравнимо с 0, 1 и 2 по модулю 16 и 5
		if ((zd & 15) > 2 || zd % 5 > 2)
			continue;

		// Как и сумма степеней 2 оставшихся коэффициентов правой части, разность не может быть сравнима
		// с 7, 8, 11 по модулю 13; с 6, 7, 10, 11 по модулю 17; и с 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0xcc0 & (1 << (zd % 17))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		k[2] = k2 * 10;
		unsigned k3 = 1;
		// Пропустим низкие значения k[3]
		for (unsigned step = k2 >> 1; step; step >>= 1)
		{
			auto f = k3 + step;
			if (zd > powers[f - 1] << 1)
				k3 = f;
		}

		worker->task->proof -= k3;

		// Перебор значений k[3]
		for (; k3 <= k2; ++k3)
		{
			const auto pk3 = powers[k3];
			if (pk3 >= zd)
				break;

			// Поиск подходящего значения коэффициента k[4]. Перед поиском хеша проверим, что разность
			// сравнима с 0 или 1 по модулю 16 (это повышает эффективность для больших коэффициентов)
			if (const NumberT lastFP = zd - pk3; !(lastFP & 14) && m_Hashes.Exists(lastFP))
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
						k[3] = k3 * 10;
						k[4] = mid * 10;
						if (OnSolutionFound(worker, k))
							return true;
						break;
					}
				}
			}
		}

		worker->task->proof += k3;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE bool Search414::SearchLast2(Worker* worker, unsigned* k, unsigned limit, NumberT z, const NumberT* powers)
{
	// Переменная z содержит значение степени нечётного коэффициента левой части, из которого вычтено
	// значение степени чётного коэффициента правой части k[1], и делённое на 5^4. Коэффициенты k[0]
	// и k[1] некратны 5, тогда как оставшиеся коэффициенты k[2], k[3] и k[4] кратны 5

	// Переменные k3 и k4 в 10 раз меньше значений k[3] и k[4], поэтому
	// в качестве их ограничения возьмём значение в 2 раза меньше limit
	const unsigned high = limit >> 1;

	// Перебор значений нечётного коэф-та k[2]
	for (unsigned k2 = 1; k2 <= limit; k2 += 2)
	{
		const auto pk2 = powers[k2];
		if (pk2 >= z)
			break;

		auto zd = z - pk2;
		// Значение k[2] нечётно, поэтому разность значений левой части уравнения и степеней коэффициентов k[1]
		// и k[2] всегда кратна 16 (как разность биквадратов нечётных чисел). И так как остальные два коэф-та
		// правой части чётные, то значение этой разности должно быть сравнимо с 0, 16 или 32 модулю 256.
		// Аналогично, разность должна быть сравнима с 0, 1 или 2 по модулю 5
		if ((zd & 255) > 32 || zd % 5 > 2)
			continue;

		// Как и сумма степеней 2 оставшихся коэффициентов правой части, разность не может быть сравнима
		// с 7, 8, 11 по модулю 13; с 6, 7, 10, 11 по модулю 17; и с 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0xcc0 & (1 << (zd % 17))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		k[2] = k2 * 5;
		zd >>= 4;

		unsigned k3 = 1;
		// Пропустим низкие значения k[3]
		for (unsigned step = high >> 1; step; step >>= 1)
		{
			auto f = k3 + step;
			if (zd > powers[f - 1] << 1)
				k3 = f;
		}

		worker->task->proof -= k3;

		// Перебор значений k[3]
		for (; k3 <= high; ++k3)
		{
			const auto pk3 = powers[k3];
			if (pk3 >= zd)
				break;

			// Поиск подходящего значения коэффициента k[4]. Перед поиском хеша проверим, что разность
			// сравнима с 0 или 1 по модулю 16 (это повышает эффективность для больших коэффициентов)
			if (const NumberT lastFP = zd - pk3; !(lastFP & 14) && m_Hashes.Exists(lastFP))
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
						k[3] = k3 * 10;
						k[4] = mid * 10;
						if (OnSolutionFound(worker, k))
							return true;
						break;
					}
				}
			}
		}

		worker->task->proof += k3;
	}

	return false;
}
