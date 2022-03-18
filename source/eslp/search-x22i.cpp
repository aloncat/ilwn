//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x22i.h"

#include "progressman.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/debug.h>
#include <core/strformat.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchX22i::IsSuitable(int power, int leftCount, int rightCount, bool allowAll)
{
	// Экспериментальный алгоритм подходит для любых степеней. Но для степеней <3 его
	// использование не имеет смысла из-за высокой скорости основного алгоритма "X22"
	return allowAll && power >= 3 && leftCount == 2 && rightCount == 2;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchX22i::GetAdditionalInfo() const
{
	Assert(m_Info.leftCount == 2 && m_Info.rightCount == 2);
	return util::Format(L"#12experimental #7algorithm for #6#X.2.2#8 (%u MiB)",
		m_SuperHashes.GetSize() / 1024);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::BeforeCompute(unsigned upperLimit)
{
	SetSearchFn(this);

	m_DecomposeFn = nullptr;
	if (m_Powers->IsType<uint64_t>())
		m_DecomposeFn = GetDecomposeFn<uint64_t>();
	else if (m_Powers->IsType<UInt128>())
		m_DecomposeFn = GetDecomposeFn<UInt128>();
	Assert(m_DecomposeFn);

	InitHashTable(m_Hashes, upperLimit);
	m_SuperHashes.Init();
	m_Tail = m_Head = 0;

	// Оптимизация для чётных степеней проверена для всех степеней до 20-й включительно. Скорее всего,
	// она работает и для других степеней, но я это не проверил (в программе степень ограничена 20-й)
	static_assert(MAX_POWER <= 20, "Need to re-check optimization for even powers for x.2.2");
	m_IsEvenPower = !(m_Info.power & 1) && (m_Info.power <= 20);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::AfterCompute()
{
	// После обработки блока заданий и остановки рабочих потоков в буфере не должно оставаться необработанных пар,
	// иначе мы можем пропустить решение. При завершении блока заданий или прерывании поиска рабочие потоки перед
	// тем, как приостановиться, должны были закончить обработку оставшихся в буфере пар
	Assert(m_Head == m_Tail && "Some pairs are still unprocessed");
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned SearchX22i::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 56000) ? 7200 :
		7200 + (56000 - hiFactor) / 8;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	// Задание задаёт только старший коэффициент левой части.
	// 2-й коэффициент левой части перебирается в SearchFactors
	Assert(!startFactors.empty());

	task.factorCount = 1;
	task.factors[0] = startFactors[0];

	const size_t len = 16;
	aux::Printc("#8\rInitializing super hashes...");

	if (m_Powers->IsType<uint64_t>())
		InitSuperHashes<uint64_t>(task.factors[0]);
	else if (m_Powers->IsType<UInt128>())
		InitSuperHashes<UInt128>(task.factors[0]);

	auto s = "#8" + std::string(len, '\b') + "...";
	s.append(len - 3, ' ');
	aux::Printc(s + "\r");
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::SelectNextTask(Task& task)
{
	++task.factors[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::WorkerFunction(Worker* worker)
{
	// Если критическая секция свободна, то любой поток её сразу захватит. Этот поток далее будет обрабатывать
	// задание, добавляя новые пары в буфер, а другие потоки будут искать декомпозиции для этих пар из буфера
	if (m_TaskCS.TryEnter())
	{
		thread::Lock lock(m_TaskCS, false);

		// Получаем задание
		if (GetNextTask(worker))
		{
			const unsigned task = worker->task.factors[0];
			(this->*m_SearchFn)(worker, m_Powers->GetData());
			lock.Leave();

			for (;;)
			{
				// Обработаем некоторую часть пар буфера. После завершения task.factors[0] будет содержать
				// значение старшего коэффициента последней обработанной пары (или 0, если пар больше нет)
				(this->*m_DecomposeFn)(worker, m_Powers->GetData());

				// Будем продолжать обработку пар в этом потоке до тех пор, пока в буфере не закончатся все
				// пары, соответствующие обработанному заданию. И только затем отметим его как завершённое
				if (unsigned k0 = worker->task.factors[0]; !k0 || k0 > task)
				{
					OnTaskDone(worker);
					return;
				}
			}
		}

		// Если заданий больше нет, то поток должен "уснуть" (после выхода из этой функции).
		// Но так как в буфере ещё могут быть необработанные пары, то поможем их обработать
		worker->shouldPause = true;
	}

	// Если секция уже была захвачена (или заданий больше нет), то
	// обрабатаем некоторое количество пар коэффициентов из буфера
	(this->*m_DecomposeFn)(worker, m_Powers->GetData());
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void SearchX22i::InitSuperHashes(unsigned startFactor)
{
	auto& console = util::SystemConsole::Instance();
	const NumberT* powers = static_cast<const NumberT*>(m_Powers->GetData());

	unsigned k0;
	// Мин. значение пары стартового задания
	const auto uv = powers[startFactor] + 1;

	// Пропустим задания, для которых макс. сумма степеней двух коэф-в
	// меньше мин. суммы степеней коэффициентов стартового задания (uv)
	for (k0 = 2; k0 < startFactor && uv > (powers[k0] << 1); ++k0);

	// Добавим в хеш-таблицу значения всех пар
	for (size_t it = 0; k0 < startFactor; ++k0)
	{
		const uint64_t pk0 = powers[k0] & ~0llu;
		const unsigned dt = (m_IsEvenPower && !(k0 & 1)) ? 2 : 1;

		for (unsigned k1 = 1; k1 <= k0; k1 += dt)
		{
			auto v = pk0 + (powers[k1] & ~0llu);
			m_SuperHashes.Insert(v);
		}

		if (!(++it & 63) && console.IsCtrlCPressed())
			break;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX22i::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Старший коэф-т левой части
	k[0] = worker->task.factors[0];
	const auto pk0 = powers[k[0]];

	// Если значение k[0] чётно, то k[1] не может быть чётным при любой чётной степени, иначе решение будет
	// непримитивным. Поэтому при чётных k[0] и степени мы будем проверять только нечётные значения k[1]
	const unsigned delta = (m_IsEvenPower && !(k[0] & 1)) ? 2 : 1;

	// Перебор 2-го коэф-та левой части
	for (k[1] = 1; k[1] <= k[0]; k[1] += delta)
	{
		// Сумма в левой части уравнения, ищем в хеш-таблице
		if (auto z = pk0 + powers[k[1]]; !m_SuperHashes.Exists(z))
		{
			// Если такого значения нет, то добавим его.
			// Решений для этой пары гарантированно нет
			m_SuperHashes.Insert(z);
			continue;
		}

		const int tail = m_Tail.load(std::memory_order_acquire);
		const int nextTail = (tail < MAX_PAIRS - 1) ? tail + 1 : 0;

		// Если буфер заполнен, то поможем другим потокам
		if (nextTail == m_Head.load(std::memory_order_relaxed))
			(this->*m_DecomposeFn)(worker, m_Powers->GetData());

		// Добавим пару
		m_Pairs[tail].k0 = k[0];
		m_Pairs[tail].k1 = k[1];
		m_Tail.store(nextTail, std::memory_order_release);

		// Вывод прогресса
		if (!(++worker->progressCounter & 0x7ff))
		{
			if (OnProgress(worker, k))
				return;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchX22i::Decompose(Worker* worker, const NumberT* powers)
{
	// Массив коэффициентов
	unsigned k[ProgressManager::MAX_COEFS];

	// Обработаем пары
	for (int count = 0; count < 32;)
	{
		// TODO: получение пары из буфера
		//

		// Сумма в левой части уравнения
		const auto z = powers[k[0]] + powers[k[1]];

		unsigned k2 = 1;
		// Пропускаем низкие значения старшего коэф-та
		for (unsigned step = k[0] >> 1; step; step >>= 1)
		{
			auto f = k2 + step;
			if (z > powers[f - 1] << 1)
				k2 = f;
		}

		for (; k2 < k[0]; ++k2)
		{
			k[2] = k2;
			if (const auto lastFP = z - powers[k2]; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k2; lo <= hi;)
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
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
FactorSearch::SearchFn SearchX22i::GetDecomposeFn()
{
	using Fn = void (SearchX22i::*)(Worker*, const NumberT*);
	auto fn = static_cast<Fn>(&SearchX22i::template Decompose);
	return reinterpret_cast<SearchFn>(fn);
}
