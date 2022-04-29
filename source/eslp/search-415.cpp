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
	return (hiFactor > 8000) ? 1600 :
		1600 + (8000 - hiFactor) / 4;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search415::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 3600) ? 4095 : (f < 9800) ? 511 : 63;
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
	static_assert(ProgressManager::MAX_COEFS >= 6,
		"Array k is too short for 4.1.5 equation");

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	if (k[0] % 5)
		SearchOdd(worker, k, powers);
	else
		Search5(worker, k, powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void Search415::Search5(Worker* worker, unsigned* k, const NumberT* powers)
{
	// Коэффициент левой части нечётен и кратен 5. Это значит, что все коэффициенты правой части должны
	// быть некратны 5, и что лишь один коэффициент справа будёт нечётным, а 4 других - чётными. Макс.
	// значениечётных 4 коэффициентов правой части не должно превышать значения коэффициента в левой
	// части. Но так как все они чётны, то мы будем перебирать значения, вдвое меньшие реальных
	const unsigned limit = k[0] >> 1;

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Перебор нечётного коэффициента k[1]
	for (unsigned nk1 = 5; k[1] < k[0]; k[1] += 2)
	{
		if (k[1] == nk1)
		{
			nk1 += 10;
			continue;
		}

		auto left = z - powers[k[1]];
		// Разность значений левой части уравнения и степени нечётного коэффициента правой части всегда
		// кратна 16 (как разность биквадратов нечётных чисел). И так как другие 4 коэффициента правой
		// части чётные, то значение разности должно быть сравнимо с 0, 16, 32, 48 или 64 модулю 256
		if ((left & 255) > 64)
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

		// Перебор значений коэффициента k[2]
		for (unsigned nk2 = k2 + 4 - (k2 + 4) % 5; k2 <= limit; ++k2)
		{
			if (k2 == nk2)
			{
				nk2 += 5;
				continue;
			}

			const auto pk2 = powers[k2];
			if (pk2 >= left)
				break;

			const auto zd = left - pk2;
			// Значение разности (также, как и суммы степеней оставшихся 3
			// коэф-тов) должно быть сравнимо с 0, 1, 2 или 3 по модулю 16
			if ((zd & 15) > 3)
				continue;

			k[2] = k2 << 1;
			if (SearchLast2(worker, k, zd, powers))
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
void Search415::SearchOdd(Worker* worker, unsigned* k, const NumberT* powers)
{
	// Коэффициент левой части нечётен и некратен 5. Это значит, что среди коэффициентов правой части лишь один будет
	// некратен 5 и один будет нечётен. Макс. значение последних 4 коэф-тов правой части не должно превышать значения
	// коэф-та в левой части. Но так все эти 4 коэф-та кратны 5, то мы будем перебирать значения, уменьшенные в 5 раз
	const unsigned limit = k[0] / 5;

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Перебор коэф-та k[1], некратного 5
	for (unsigned nk1 = 5; k[1] < k[0]; ++k[1])
	{
		if (k[1] == nk1)
		{
			nk1 += 5;
			continue;
		}

		auto left = z - powers[k[1]];
		// Так как все 4 оставшиеся коэффициента правой части кратны 5, то их сумма кратна
		// 5^4, а значит и разность тоже должна быть кратна 5^4, иначе решений не будет
		if (left % 625)
			continue;

		if (k[1] & 1)
		{
			// Если значение k[1] нечётно, то разность значений левой части уравнения и степени коэффициента k[1]
			// всегда кратна 16 (как разность биквадратов нечётных чисел). И так как остальные 4 коэф-та правой
			// части чётные, то значение этой разности должно быть сравнимо с 0, 16, 32, 48 или 64 модулю 256
			if ((left & 255) > 64)
				continue;

			left = (left / 625) >> 4;
			const unsigned lim2 = limit >> 1;

			// Перебор старшего чётного коэф-та k[2]
			for (unsigned k2 = 1; k2 <= lim2; ++k2)
			{
				const auto pk2 = powers[k2];
				if (pk2 >= left)
					break;

				const auto zd = left - pk2;
				// Значение разности (также, как и суммы степеней оставшихся трёх
				// коэф-тов) должно быть сравнимо с 0, 1, 2 или 3 по модулю 16 и 5
				if ((zd & 15) > 3 || zd % 5 > 3)
					continue;

				k[2] = k2 * 10;
				if (SearchLast10(worker, k, zd, lim2, powers))
					return;
			}
		} else
		{
			left /= 625;

			// Перебор нечётного коэф-та k[2]
			for (unsigned k2 = 1; k2 <= limit; k2 += 2)
			{
				const auto pk2 = powers[k2];
				if (pk2 >= left)
					break;

				auto zd = left - pk2;
				// Сейчас в правой части остаётся три чётных коэффициента, поэтому разность должна
				// быть сравнима с 0, 16, 32 или 48 по модулю 256, а также 0, 1, 2 или 3 по модулю 5
				if ((zd & 255) > 48 || zd % 5 > 3)
					continue;

				k[2] = k2 * 5;
				if (SearchLast10(worker, k, zd >> 4, limit >> 1, powers))
					return;
			}
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
AML_NOINLINE bool Search415::SearchLast2(Worker* worker, unsigned* k, NumberT z, const NumberT* powers)
{
	// Переменная z содержит значение степени нечётного коэффициента левой части (кратного 5), из которого
	// вычтено значение степени единственного нечётного коэф-та правой части k[1] (некратного 5), а также
	// значение степени старшего чётного коэф-та k[2], и делённое на 16. Коэффициенты k[2], k[3], k[4] и
	// k[5] - это чётные некратные 5 коэффициенты правой части

	unsigned k3 = 1;
	// Пропустим низкие значения k[3]
	for (unsigned step = k[2] >> 2; step; step >>= 1)
	{
		auto f = k3 + step;
		if (z > powers[f - 1] * 3)
			k3 = f;
	}

	const unsigned k2 = k[2] >> 1;
	// Перебор значений коэффициента k[3]
	for (unsigned nk3 = k3 + 4 - (k3 + 4) % 5; k3 <= k2; ++k3)
	{
		if (k3 == nk3)
		{
			nk3 += 5;
			continue;
		}

		const auto pk3 = powers[k3];
		if (pk3 >= z)
			break;

		const auto zd = z - pk3;
		// Сейчас в правой части остаётся два коэффициента, поэтому
		// разность должна быть сравнима с 0, 1 или 2 по модулю 16
		if ((zd & 15) > 2)
			continue;

		// Как и сумма степеней 2 оставшихся коэффициентов правой части, разность не может быть сравнима
		// с 7, 8, 11 по модулю 13; с 6, 7, 10, 11 по модулю 17; и с 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0xcc0 & (1 << (zd % 17))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		k[3] = k3 << 1;
		unsigned k4 = 1;
		// Пропустим низкие значения k[4]
		for (unsigned step = k3 >> 1; step; step >>= 1)
		{
			auto f = k4 + step;
			if (zd > powers[f - 1] << 1)
				k4 = f;
		}

		worker->task->proof -= k4;

		// Перебор значений k[4]
		for (unsigned nk4 = k4 + 4 - (k4 + 4) % 5; k4 <= k3; ++k4)
		{
			if (k4 == nk4)
			{
				nk4 += 5;
				continue;
			}

			const auto pk4 = powers[k4];
			if (pk4 >= zd)
				break;

			if (const NumberT lastFP = zd - pk4; !(lastFP & 14) && m_Hashes.Exists(lastFP))
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
						k[4] = k4 << 1;
						k[5] = mid << 1;
						if (OnSolutionFound(worker, k))
							return true;
						break;
					}
				}
			}
		}

		worker->task->proof += k4;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE bool Search415::SearchLast10(Worker* worker, unsigned* k, NumberT z, unsigned limit, const NumberT* powers)
{
	// Переменная z содержит значение степени нечётного коэффициента левой части (некратного 5), из которого вычтено
	// значение степени единственного некратного 5 коэф-та правой части k[1], а также значение степени коэф-та k[2],
	// и делённое на 16 и 5^4. Коэффициенты k[2], k[3], k[4] и k[5] - это кратные 5 коэффициенты правой части,
	// причём все, кроме k[2], всегда чётные. Коэффициент k[2] чётен тогда, когда k[1] нечётен и наоборот

	unsigned k3 = 1;
	// Пропустим низкие значения коэффициента k[3]
	for (unsigned step = limit >> 1; step; step >>= 1)
	{
		auto f = k3 + step;
		if (z > powers[f - 1] * 3)
			k3 = f;
	}

	// Перебор значений k[3]
	for (; k3 <= limit; ++k3)
	{
		const auto pk3 = powers[k3];
		if (pk3 >= z)
			break;

		const auto zd = z - pk3;
		// Значение разности (также, как и суммы степеней оставшихся двух
		// коэф-тов) должно быть сравнимо с 0, 1 или 2 по модулю 16 и 5
		if ((zd & 15) > 2 || zd % 5 > 2)
			continue;

		// Как и сумма степеней 2 оставшихся коэффициентов правой части, разность не может быть сравнима
		// с 7, 8, 11 по модулю 13; с 6, 7, 10, 11 по модулю 17; и с 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0xcc0 & (1 << (zd % 17))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		k[3] = k3 * 10;
		unsigned k4 = 1;
		// Пропустим низкие значения k[4]
		for (unsigned step = k3 >> 1; step; step >>= 1)
		{
			auto f = k4 + step;
			if (zd > powers[f - 1] << 1)
				k4 = f;
		}

		worker->task->proof -= k4;

		// Перебор значений k[4]
		for (; k4 <= k3; ++k4)
		{
			const auto pk4 = powers[k4];
			if (pk4 >= zd)
				break;

			if (const NumberT lastFP = zd - pk4; !(lastFP & 14) && m_Hashes.Exists(lastFP))
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
						k[4] = k4 * 10;
						k[5] = mid * 10;
						if (OnSolutionFound(worker, k))
							return true;
						break;
					}
				}
			}
		}

		worker->task->proof += k4;
	}

	return false;
}
