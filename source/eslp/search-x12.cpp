//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x12.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX12::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 1 && m_Info.rightCount == 2);
	return L"#15optimized #7algorithm for #6#X.1.2";
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX12::BeforeCompute(unsigned upperLimit)
{
	FactorSearch::BeforeCompute(upperLimit);
	SetSearchFn(this);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX12::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX12::MightHaveSolution(const Task& task) const
{
	// Все чётные степени от 2 до 20 включительно
	if (!(m_Info.power & 1) && m_Info.power <= 20)
	{
		// Z не может быть чётным для p.1.2 и p.1.3 (для чётных p).
		// NB: все эти оптимизации используются также и для x.1.3
		if (!(task.factors[0] & 1))
			return false;

		if (m_Info.power == 2)
		{
			if (m_Info.rightCount == 2 && !(task.factors[0] % 3))
				return false;
		}
		else if (m_Info.power == 4)
		{
			if (m_Info.rightCount == 2 && !(task.factors[0] % 3))
				return false;
			if (!(task.factors[0] % 5))
				return false;
		}
		else if (m_Info.power == 6)
		{
			if (!(task.factors[0] % 3) || !(task.factors[0] % 7))
				return false;
		}
		else if (m_Info.power == 8)
		{
			if (!(task.factors[0] % 5))
				return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX12::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task.factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * 2)
			k[1] = f;
	}

	for (; k[1] < k[0]; ++k[1])
	{
		if (const auto lastFP = z - powers[k[1]]; m_Hashes.Exists(lastFP))
		{
			for (unsigned lo = 1, hi = k[1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[2] = mid;
					if (OnSolutionFound(worker, k))
						return;
					break;
				}
			}
		}
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & 0xff))
		OnProgress(worker, k);
}
