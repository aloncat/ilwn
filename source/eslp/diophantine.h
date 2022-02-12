//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "hashtable.h"
#include "solution.h"
#include "uint128.h"

#include <core/file.h>
#include <core/platform.h>
#include <core/threadsync.h>
#include <core/util.h>

#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch - поиск коэффициентов уравнения
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class FactorSearch
{
	AML_NONCOPYABLE(FactorSearch)

public:
	static constexpr int MAX_POWER = 20;
	static constexpr int MAX_FACTOR_COUNT = 160;

	FactorSearch();
	~FactorSearch();

	bool Run(int power, int count, unsigned hiFactor);

protected:
	struct Worker;

	// Ищет решения, начиная с hiFactor и до maxFactor
	void Search(unsigned hiFactor, unsigned maxFactor);

	// Создаёт указанное количество рабочих потоков в приостановленном состоянии
	void CreateWorkers(size_t threadCount);
	// Устанавливает всем рабочим потокам флаг shouldQuit и дожидается их завершения
	void KillWorkers();

	// Возвращает количество активных (работающих) потоков
	size_t GetActiveThreads(bool ignorePending = false) const;
	// Запускает (приостанавливает) часть рабочих потоков так, чтобы
	// количество активных стало равно указанному значению activeCount
	void SetActiveThreads(size_t activeCount);

	// Выполняет вычисления, начиная от hiFactor, возвращает следующее
	// за последним проверенным значение старшего коэффициента
	unsigned Compute(unsigned hiFactor);

	template<class NumberT>
	unsigned Compute(unsigned hiFactor, unsigned toCheck);

	template<class NumberT>
	const NumberT*& GetPowersArray();

	// Главная функция рабочего потока
	void WorkerMainFn(Worker* worker);

	//
	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	//
	void OnProgress(int id, const unsigned* factors);
	void OnSolutionFound(const unsigned* factors);

	void GetProgress(unsigned* factors);

//-->
	bool GetNextTask(Worker* worker);
	void OnTaskDone(Worker* worker);
//<--

protected:
	int m_Power = 0;						// Степень уравнения (от 1 до MAX_POWER)
	int m_FactorCount = 0;					// Количество коэффициентов в правой части

	thread::CriticalSection m_TaskCS;		// Критическая секция (задания)
	thread::CriticalSection m_ProgressCS;	// Критическая секция (прогресс)

	std::vector<Worker*> m_Workers;			// Рабочие потоки
	size_t m_MaxWorkerCount = 0;			// Максимальное количество активных потоков
	size_t m_ActiveWorkers = 0;				// Установленное количество активных потоков

	util::BinaryFile m_Log;					// Файл журнала
	Solutions m_Solutions;					// Найденные решения

	const uint64_t* m_Pow64 = nullptr;		// Массив степеней (64 бита)
	const UInt128* m_Powers = nullptr;		// Массив степеней (128+ бит)
	HashTable m_Hashes;						// Хеш-таблица значений степеней

	volatile bool m_IsCancelled = false;	// true, если пользователь прервал работу программы

//-->
	volatile unsigned m_NextFactor = 0;
	unsigned m_LastFactor = 0;

	unsigned m_Progress[8];
	std::vector<unsigned> m_PendingFactors;
	int m_LowestWorkerId = 0;
//<--
};
