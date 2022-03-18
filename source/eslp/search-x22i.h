//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"
#include "hashtable.h"

#include <core/threadsync.h>

#include <atomic>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX22 - экспериментальный алгоритм для уравнений x.2.2
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX22i : public FactorSearch
{
public:
	SearchX22i();

protected:
	static bool IsSuitable(int power, int leftCount, int rightCount, bool allowAll);
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;
	virtual void AfterCompute() override;

	virtual unsigned GetChunkSize(unsigned hiFactor) override;

	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	virtual void WorkerFunction(Worker* worker) override;

	template<class NumberT>
	void InitSuperHashes(unsigned startFactor);

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	template<class NumberT>
	void Decompose(Worker* worker, const NumberT* powers);

	template<class NumberT>
	static SearchFn GetDecomposeFn();

protected:
	// Макс. количество пар в m_Pairs
	static constexpr int MAX_PAIRS = 2048;

	struct Pair {
		unsigned k0;	// Старший коэффициент левой части
		unsigned k1;	// Младший коэффициент левой части
	};

	HashTable<23> m_Hashes;				// Обычная хеш-таблица (для поиска коэффициента)
	thread::CriticalSection m_TaskCS;	// Критическая секция для потока задания
	SuperHashTable<35> m_SuperHashes;	// Большая хеш-таблица для значений пар
	SearchFn m_DecomposeFn = nullptr;	// Указатель на функцию декомпозиции

	Pair m_Pairs[MAX_PAIRS];			// Кольцевой буфер пар коэффициентов
	std::atomic<int> m_Head;			// Индекс первого элемента (голова списка)
	std::atomic<int> m_Tail;			// Индекс элемента, следующего за последним

	bool m_IsEvenPower = false;			// true, если степень уравнения чётная
};
