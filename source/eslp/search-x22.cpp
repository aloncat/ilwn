//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x22.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX22::GetAdditionalInfo() const
{
	Assert(m_Info.power <= 20 && m_Info.leftCount == 2 && m_Info.rightCount == 2);
	return L"#15optimized #7algorithm for #6#X.2.2";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::InitHashTable(PowersBase& powers, unsigned upperLimit)
{
	if (m_Pow64)
		m_Hashes.Init(upperLimit, static_cast<Powers<uint64_t>&>(powers));
	else
		m_Hashes.Init(upperLimit, static_cast<Powers<UInt128>&>(powers));
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	// Для x.2.2 задание задаёт только старший коэффициент левой части. 2-й коэффициент
	// в левой части, так же, как и коэффициенты в правой, перебираются в SearchFactors
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22::PerformTask(Worker* worker)
{
	if (m_Pow64)
		SearchFactors(worker, m_Pow64);
	else
		SearchFactors(worker, m_Powers);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX22::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Старший коэф-т левой части
	k[0] = worker->task.factors[0];

	// Если значение k[0] чётно, то k[1] не может быть чётным при любой чётной степени, иначе решение будет
	// непримитивным. Поэтому при чётных k[0] и степени мы будем проверять только нечётные значения k[1]
	const unsigned delta = ((m_Info.power & 1) || (k[0] & 1)) ? 1 : 2;

	// Перебор 2-го коэф-та левой части
	for (k[1] = 1; k[1] <= k[0]; k[1] += delta)
	{
		// Сумма в левой части уравнения
		const auto z = powers[k[0]] + powers[k[1]];

		k[2] = 1;
		// Пропускаем низкие значения старшего коэф-та
		for (unsigned step = k[0] >> 1; step; step >>= 1)
		{
			auto f = k[2] + step;
			if (z > powers[f - 1] * 2)
				k[2] = f;
		}

		for (; k[2] < k[0]; ++k[2])
		{
			if (const auto lastFP = z - powers[k[2]]; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k[2]; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[3] = mid;
						if (OnSolutionFound(worker, k))
							return;
						break;
					}
				}
			}
		}

		// Вывод прогресса
		if (!(++worker->progressCounter & 0x7ff))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}
