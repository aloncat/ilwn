﻿//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "hashtable.h"
#include "searchbase.h"
#include "solution.h"
#include "threadtimer.h"
#include "uint128.h"

#include <core/file.h>
#include <core/platform.h>
#include <core/threadsync.h>
#include <core/util.h>

#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch - поиск коэффициентов диофантова уравнения вида p.m.n
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class FactorSearch : public SearchBase
{
public:
	FactorSearch();
	virtual ~FactorSearch() override;

protected:
	struct Worker;

	// Выполняет поиск решений (начиная с указанных значений коэффициентов)
	virtual void Search(const std::vector<unsigned>& startFactors) override;

	// Создаёт указанное количество рабочих потоков в приостановленном состоянии
	void CreateWorkers(size_t threadCount);
	// Устанавливает всем рабочим потокам флаг shouldQuit и дожидается их завершения
	void KillWorkers();

	// Возвращает количество активных (работающих) потоков. Если ignorePending == false, то
	// активные потоки, для которых установлен флаг shouldPause, не включаются в результат
	size_t GetActiveThreads(bool ignorePending = false) const;
	// Запускает или приостанавливает часть рабочих потоков так, чтобы
	// количество активных стало равно указанному значению activeCount
	void SetActiveThreads(size_t activeCount);
	// Возвращает количество затраченного всеми потоками времени CPU (в
	// миллисекундах) с момента запуска или последнего вызова функции
	uint64_t GetThreadTimes();

	// Выполняет вычисления, начиная от hiFactor, возвращает следующее
	// за последним проверенным значение старшего коэффициента
	unsigned Compute(unsigned hiFactor);

	template<class NumberT>
	unsigned Compute(unsigned hiFactor, unsigned toCheck);

	// Главная функция рабочего потока
	void WorkerMainFn(Worker* worker);

	template<class NumberT>
	const NumberT*& PowersArray();

	void GetProgress(unsigned* factors);

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	bool GetNextTask(Worker* worker);
	void OnTaskDone(Worker* worker);

	// Проверяет, могут ли существовать решения для задания, содержащегося в worker.
	// Если гарантируется, что решений быть не может, то функция вернёт false
	bool MightHaveSolution(const Worker* worker);

	// Функция врнёт true, если установлен флаг m_ForceQuit
	bool OnProgress(Worker* worker, const unsigned* factors);

	void OnSolutionFound(const unsigned* factors);
	void OnSolutionReady(const Solution& solution);
	void ProcessPendingSolutions();

	void ShowProgress(const unsigned* factors);
	virtual void UpdateConsoleTitle() override;
	void UpdateActiveThreadCount();

protected:
	thread::CriticalSection m_TaskCS;			// Критическая секция (задания)
	thread::CriticalSection m_ProgressCS;		// Критическая секция (прогресс)
	thread::CriticalSection m_ConsoleCS;		// Критическая секция (вывод в консоль)

	ThreadTimer m_MainThreadTimer;				// Счётчик времени CPU для главного потока
	std::vector<Worker*> m_Workers;				// Состояния рабочих потоков
	size_t m_ActiveWorkers = 0;					// Желаемое количество активных потоков

	Solutions m_Solutions;						// Найденные примитивные решения
	volatile size_t m_SolutionsFound = 0;		// Количество найденных примитивных решений
	std::vector<Solution> m_PendingSolutions;	// Найденные решения, ожидающие проверки

	const uint64_t* m_Pow64 = nullptr;			// Массив степеней (64 бита)
	const UInt128* m_Powers = nullptr;			// Массив степеней (128 бит)
	HashTable m_Hashes;							// Хеш-таблица значений степеней

	volatile unsigned m_NextHiFactor = 0;		// Следующий старший коэффициент (задание)
	unsigned m_LastDoneFactor = 0;				// Последний проверенный коэффициент (задание)
	unsigned m_LastHiFactor = 0;				// Последний старший коэффициент в блоке заданий

	std::vector<unsigned> m_PendingTasks;		// Выполняемые и ожидающие проверки задания
	volatile unsigned m_LoPendingTask = 0;		// Самое младшее (старое) ожидаемое задание

	unsigned m_Progress[8];						// Первые коэффициенты для вывода прогресса
	size_t m_LastProgressLength = 0;			// Количество символов в последнем выводе прогресса
	volatile bool m_IsProgressReady = false;	// true, если данные о прогрессе можно использовать
	volatile bool m_ForceShowProgress = false;	// true, если прогресс нужно вывести немедленно
	volatile bool m_NeedUpdateTitle = false;	// true, если нужно обновить заголовок окна

	volatile bool m_NoTasks = false;			// true, если заданий для потоков больше нет
	volatile bool m_IsCancelled = false;		// true, если работа была прервана пользователем
	volatile bool m_ForceQuit = false;			// true, если нужно прервать работу немедленно
};
