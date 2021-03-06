//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-413.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool Search413::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	return power == 4 && leftCount == 1 && rightCount == 3;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Search413::GetAdditionalInfo() const
{
	Assert(m_Info.power == 4 && m_Info.leftCount == 1 && m_Info.rightCount == 3);
	return L"#15specialized #7algorithm for #6#4.1.3";
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::BeforeCompute(unsigned upperLimit)
{
	SetSearchFn(this);
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned Search413::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 12000000) ? 3000000 : 6000000;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	// Z должен быть сравним с 1 по модулю 8 и не кратен 5
	while ((task.factors[0] & 7) != 1 || !(task.factors[0] % 5))
		++task.factors[0];

	const auto f = task.factors[0];
	m_ProgressMask = (f < 900000) ? 63 : (f < 2500000) ? 7 : 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Search413::SelectNextTask(WorkerTask& task)
{
	task.factors[0] += 8;
	if (!(task.factors[0] % 5))
		task.factors[0] += 8;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void Search413::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];
	static_assert(ProgressManager::MAX_COEFS >= 4,
		"Array k is too short for 4.1.3 equation");

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	// Макс. значение последних 2 коэффициентов правой части не должно превышать значения коэффициента
	// в левой части. Но так как они оба кратны 8, то мы будем перебирать значения, уменьшенные в 8 раз
	unsigned limit = k[0] >> 3;

	// Так как сумма или разность k[0] и k[1] должна быть кратна 1024, то нет смысла проверять каждое
	// нечётное значение k[1] в цикле. Из каждых 1024 последовательных чисел нам будут нужны только 2:
	// то, которое в сумме с k[0] будет кратно 1024, и то, для которого кратно 1024 будет k[0] - k[1]
	auto f1 = k[0] & 1023;
	f1 = (f1 < 512) ? f1 : 1024 - f1;
	const unsigned delta[] = { 1024 - 2 * f1, 2 * f1 };

	const bool zm3 = !(k[0] % 3);
	for (unsigned k1 = f1, dt = 0; k1 < k[0]; k1 += delta[dt], dt ^= 1)
	{
		k[1] = k1;
		uint64_t t = k1;
		NumberT pk1 = t *= t;
		auto zd = z - pk1 * t;

		// Разность значений левой части уравнения и степени первого коэффициента правой части всегда кратна
		// 4096 (как разность биквадратов, учитывая условие выше). И так как другие 2 коэффициента правой
		// части кратны 8, то значение разности должно быть сравнимо с 0, 4096 или 8192 модулю 65536
		if ((zd & 65535) > 8192)
			continue;

		if (zm3)
		{
			// Если коэффициент в левой части кратен 3, то ни один
			// из коэффициентов правой части не может быть кратен 3
			if (!(k1 % 3))
				continue;
		}
		// Если коэффициент в левой части и нечётный коэффициент правой не кратны 3, то другие
		// 2 коэф-та правой части будут кратны 3, а значит и разность должна быть кратна 3^4
		else if (k1 % 3 && zd % 81)
			continue;

		// Если k[1] не кратен 5, то оставшиеся 2 коэффициента правой части должны быть кратны 5.
		// В этом случае значение zd должно быть кратно 5^4. Если это не так, то решений не будет
		if (k1 % 5 && zd % 625)
			continue;

		// Также разность (как и сумма степеней оставшихся коэффициентов правой части) не может
		// быть сравнима с 7, 8, 11 по модулю 13, а также 4, 5, 6, 9, 13, 22, 28 по модулю 29
		if ((0x980 & (1 << (zd % 13))) || (0x10402270 & (1 << (zd % 29))))
			continue;

		zd >>= 12;
		unsigned k2 = limit, k3 = 1;
		while (zd <= powers[k2]) --k2;
		limit = k2;

		auto pk2 = powers[k2];
		for (unsigned step = k2 >> 1; step; step >>= 1)
		{
			auto f = k3 + step;
			if (zd > pk2 + powers[f - 1])
				k3 = f;
		}

		for (; k2 >= k3; ++k3)
		{
			if (auto sum = pk2 + powers[k3]; sum >= zd)
			{
				if (sum == zd)
				{
					k[2] = k2 << 3;
					k[3] = k3 << 3;
					if (OnSolutionFound(worker, k))
						return;
				} else
				{
					pk2 = powers[--k2];
				}
			}
		}

		worker->task->proof += k3;
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & m_ProgressMask))
		OnProgress(worker, k);
}
