//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x12.h"

#include "progressman.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX12::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	return leftCount == 1 && rightCount == 2;
}

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
	m_CheckTaskFn = GetCheckTaskFn();
}

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::CheckTaskFn SearchX12::GetCheckTaskFn() const
{
	// Оптимизаций для нечётных степеней и степеней выше 20-й нет.
	// Часть перечисленных оптимизаций используется также для x.1.3
	if ((m_Info.power & 1) || m_Info.power > 20)
		return [](const WorkerTask&) { return true; };

	// Уравнение 2.1.2: Z не может быть чётным и кратным 3
	if (m_Info.power == 2 && m_Info.rightCount == 2)
	{
		return [](const WorkerTask& task) {
			return (task.factors[0] & 1) && (task.factors[0] % 3);
		};
	}

	// Уравнения 4.1.n и 8.1.n: Z не может быть чётным для n < 16 (32); Z не
	// может быть кратным 3 для n < 3; Z не может быть кратным 5 для n < 5
	if (m_Info.power == 4 || m_Info.power == 8)
	{
		if (m_Info.rightCount < 3)
		{
			return [](const WorkerTask& task) {
				return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 5);
			};
		}

		return [](const WorkerTask& task) {
			return (task.factors[0] & 1) && (task.factors[0] % 5);
		};
	}

	// Уравнение 6.1.n: Z не может быть чётным для n < 8; Z не может
	// быть кратным 3 для n < 9; Z не может быть кратным 7 для n < 7
	if (m_Info.power == 6)
	{
		return [](const WorkerTask& task) {
			return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 7);
		};
	}

	// Другие случаи: Z не может быть чётным
	return [](const WorkerTask& task) {
		return (task.factors[0] & 1) != 0;
	};
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX12::SelectNextTask(WorkerTask& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX12::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Коэффициент левой части
	k[0] = worker->task->factors[0];

	// Левая часть уравнения
	const auto z = powers[k[0]];

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] << 1)
			k[1] = f;
	}

	const auto proof = k[0] - k[1];
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

	worker->task->proof += proof;

	// Вывод прогресса
	if (!(++worker->progressCounter & 0xff))
		OnProgress(worker, k);
}
