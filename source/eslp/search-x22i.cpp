//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-x22i.h"

#include "progressman.h"
#include "uint128.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/debug.h>
#include <core/strformat.h>
#include <core/thread.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
SearchX22i::~SearchX22i()
{
	AML_SAFE_DELETEA(m_WorkerPairs);
}

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

	memset(m_Pairs, 0, sizeof(m_Pairs));
	m_Tail = m_Head = 0;
	m_Lock = 0;

	auto threads = GetMaxThreadCount();
	m_WorkerPairs = new unsigned[threads + 1];
	memset(m_WorkerPairs, 0, 4 * (threads + 1));

	// Оптимизация для чётных степеней проверена для всех степеней до 20-й включительно. Скорее всего,
	// она работает и для других степеней, но я это не проверил (в программе степень ограничена 20-й)
	static_assert(MAX_POWER <= 20, "Need to re-check optimization for even powers for x.2.2");
	m_IsEvenPower = !(m_Info.power & 1) && (m_Info.power <= 20);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchX22i::AfterCompute()
{
	AML_SAFE_DELETEA(m_WorkerPairs);

	// После обработки блока заданий и остановки рабочих потоков в буфере не должно оставаться необработанных пар,
	// иначе мы можем пропустить решение. При завершении блока заданий или прерывании поиска рабочие потоки перед
	// тем, как приостановиться, должны были закончить обработку оставшихся в буфере пар
	Assert(m_Head == m_Tail && "Some pairs are still unprocessed");
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned SearchX22i::GetChunkSize(unsigned hiFactor)
{
	return (hiFactor > 72000) ? 7600 :
		7600 + (72000 - hiFactor) / 8;
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

		// Получим задание
		if (GetNextTask(worker))
		{
			// Обработаем полученное задание
			const unsigned task = worker->task.factors[0];
			(this->*m_SearchFn)(worker, m_Powers->GetData());
			// И сразу же освободим критическую секцию
			lock.Leave();

			for (;;)
			{
				// Обработаем немного пар из буфера. После завершения поле worker->userData будет содержать
				// значение старшего коэф-та последней обработанной потоком пары (или 0, если пар больше нет)
				(this->*m_DecomposeFn)(worker, m_Powers->GetData());

				// Будем продолжать обработку пар в этом потоке до тех пор, пока в буфере не закончатся все
				// пары, соответствующие обработанному заданию. И только затем отметим его как завершённое
				if (unsigned k0 = worker->userData; !k0 || k0 > task)
				{
					// Другие потоки всё ещё могут обрабатывать пары нашего задания (уже извлечённые из буфера).
					// Поэтому перед тем, как "завершить" задание, убедимся, что они уже закончили их обработку
					if (k0 = GetLowestPair(); !k0 || k0 > task)
					{
						OnTaskDone(worker);
						return;
					}
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
unsigned SearchX22i::GetLowestPair() const
{
	unsigned lowest = 0;
	unsigned k0 = UINT_MAX;

	auto threads = GetMaxThreadCount();
	for (size_t id = 1; id <= threads; ++id)
	{
		if (unsigned k = m_WorkerPairs[id]; k && k < k0)
			lowest = k0 = k;
	}

	return lowest;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
FactorSearch::SearchFn SearchX22i::GetDecomposeFn()
{
	using Fn = void (SearchX22i::*)(Worker*, const NumberT*);
	auto fn = static_cast<Fn>(&SearchX22i::template Decompose);
	return reinterpret_cast<SearchFn>(fn);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void SearchX22i::InitSuperHashes(unsigned startFactor)
{
	auto& console = util::SystemConsole::Instance();
	const NumberT* powers = static_cast<const NumberT*>(m_Powers->GetData());

	unsigned k0;
	const auto uv = powers[startFactor] + 1;
	// Пропустим задания, для которых макс. сумма степеней двух коэф-в
	// меньше мин. суммы степеней коэффициентов стартового задания uv
	for (k0 = 2; k0 < startFactor && uv > (powers[k0] << 1); ++k0);

	// Добавим в хеш-таблицу значения всех пар. Зная, что хеш - это N бит суммы (биты 12..46), вместо нахождения
	// реальной (128-битной для UInt128) суммы, будет достаточно сложить лишь младшие 64 бита двух коэффициентов
	for (size_t it = 0; k0 < startFactor; ++k0)
	{
		const uint64_t pk0 = powers[k0] & ~0llu;
		const unsigned dt = (m_IsEvenPower && !(k0 & 1)) ? 2 : 1;

		for (unsigned k1 = 1; k1 <= k0; k1 += dt)
		{
			auto v = pk0 + (powers[k1] & ~0llu);
			m_SuperHashes.Insert(v);
		}

		// Если пользователь нажмёт Ctrl-C в процессе подготовки хеш-таблицы, то корректная
		// работа алгоритма будет невозможна. Поэтому в этом случае поиск не будет начат
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
	const uint64_t pk0 = powers[k[0]] & ~0llu;

	// Если значение k[0] чётно, то k[1] не может быть чётным при любой чётной степени, иначе решение будет
	// непримитивным. Поэтому при чётных k[0] и степени мы будем проверять только нечётные значения k[1]
	const unsigned delta = (m_IsEvenPower && !(k[0] & 1)) ? 2 : 1;

	// Перебор 2-го коэф-та левой части
	for (k[1] = 1; k[1] <= k[0]; k[1] += delta)
	{
		// Сумма в левой части уравнения (младшие 64 бита); ищем в хеш-таблице
		if (auto v = pk0 + (powers[k[1]] & ~0llu); !m_SuperHashes.Exists(v))
		{
			// Если такого значения нет, то добавим.
			// Решений для пары гарантированно нет
			m_SuperHashes.Insert(v);
			continue;
		}

		const int tail = m_Tail.load(std::memory_order_acquire);
		const int nextTail = (tail < MAX_PAIRS - 1) ? tail + 1 : 0;

		// Если буфер заполнен (голова списка находится сразу за хвостом), то поможем другим потокам (обработаем часть
		// пар сами), освободив место в буфере. Перед записью в хвостовой элемент буфера мы должны убедиться, что другой
		// поток, переместив голову списка с этого элемента, обнулил значение k0 после чтения пары. Если он этого ещё не
		// сделал, то мы также должны подождать. Ждать необходимо в цикле, т.к. обработка нами нескольких пар из буфера
		// не гарантирует, что поток, читающий сейчас хвостовой элемент, завершит за это время свою операцию
		while (nextTail == m_Head.load(std::memory_order_relaxed) || m_Pairs[tail].k0)
			(this->*m_DecomposeFn)(worker, m_Powers->GetData());
		std::atomic_thread_fence(std::memory_order_acquire);

		// Сохраним пару
		m_Pairs[tail].k0 = k[0];
		m_Pairs[tail].k1 = k[1];
		// И переместим вправо хвост списка
		m_Tail.store(nextTail, std::memory_order_release);

		// Вывод прогресса
		if (!(++worker->progressCounter & 0xfff))
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

	// Обработаем некоторое количество пар
	for (int count = 0; count < 64; ++count)
	{
		// Захватим спин-лок перед тем, как извлечь из буфера следующую пару для обработки
		for (int lock = 0; !m_Lock.compare_exchange_weak(lock, 1, std::memory_order_acquire);)
		{
			thread::CPUPause();
			lock = 0;
		}

		// Проверим, что буфер не пуст
		const int head = m_Head.load(std::memory_order_relaxed);
		if (head == m_Tail.load(std::memory_order_acquire))
		{
			// Буфер пуст, работы для нас нет
			m_Lock.store(0, std::memory_order_relaxed);
			m_WorkerPairs[worker->workerId] = 0;
			worker->userData = 0;
			return;
		}

		// Читаем значение пары
		const Pair pair = m_Pairs[head];
		// Перед изменением головы списка, загрузим в m_WorkerPairs знчение старшего коэффициента пары,
		// которую мы планируем обработать. Важно сделать это до освобождения спин-лока, иначе функция
		// GetLowestPair при определённых обстоятельствах может посчитать эту пару обработанной
		m_WorkerPairs[worker->workerId] = pair.k0;

		// Перемещаем голову и освобождаем спин-лок
		const int nextHead = (head < MAX_PAIRS - 1) ? head + 1 : 0;
		m_Head.store(nextHead, std::memory_order_relaxed);
		m_Lock.store(0, std::memory_order_release);

		// Голова списка перемещена, значение пары загружено. Чтобы
		// элемент буфера мог быть использован снова, "обнулим" его
		std::atomic_thread_fence(std::memory_order_release);
		m_Pairs[head].k0 = 0;

		k[0] = pair.k0;
		k[1] = pair.k1;

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
				for (unsigned lo = k[1] + 1, hi = k2; lo <= hi;)
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
						{
							m_WorkerPairs[worker->workerId] = 0;
							worker->userData = 0;
							return;
						}
						break;
					}
				}
			}
		}
	}

	// Обработка пар потоком завершена
	m_WorkerPairs[worker->workerId] = 0;
	worker->userData = k[0];
}
