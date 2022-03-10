//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "hashtable.h"
#include "powers.h"
#include "progressman.h"
#include "searchbase.h"
#include "solution.h"
#include "threadtimer.h"
#include "uint128.h"

#include <core/platform.h>
#include <core/threadsync.h>

#include <thread>
#include <utility>
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
	struct Task;
	struct Worker;

	// Максимальное количество выводимых решений
	static constexpr size_t MAX_SOLUTIONS = 100000;

	// Выполняет поиск решений (начиная с указанных значений коэффициентов)
	virtual void Search(const Options& options, const std::vector<unsigned>& startFactors) override;

	// Обновляет заголовок окна консоли
	virtual void UpdateConsoleTitle() override;

	// Инициализирует хеш-таблицу
	virtual void InitHashTable(PowersBase& powers, unsigned upperLimit);

	// Инициализирует начальное задание task из набора коэфициентов startFactors
	// и устанавливает количество коэффициентов, которые будут задаваться заданием
	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors);

	// Выбирает следующее задание
	virtual void SelectNextTask(Task& task);

	// Возвращает значение между указанным стартовым значением старшего коэффициента левой части hiFactor
	// и желаемым конечным значением в блоке вычислений. Влияет на количество инициализируемых элементов
	// в массиве степеней и количество инициализируемых значений в хеш-таблице
	virtual unsigned GetChunkSize(unsigned hiFactor);

	// Проверяет, могут ли существовать решения для указанного задания task. Если
	// гарантируется, что решений быть не может, то функция должна вернуть false
	virtual bool MightHaveSolution(const Task& task) const { return true; }

	// Выполняет задание в рабочем потоке
	virtual void PerformTask(Worker* worker) = 0;

	template<class NumberT>
	const NumberT*& PowersArray();

	// Функция вернёт true, если установлен флаг m_ForceQuit.
	// Вызывается рабочим потоком для вывода прогресса поиска
	bool OnProgress(const Worker* worker, const unsigned* factors);

	// Эта функция вызывается рабочим потоком при нахождении решения
	bool OnSolutionFound(const Worker* worker, const unsigned* factors);

protected:
	const uint64_t* m_Pow64 = nullptr;		// Массив степеней (64 бита)
	const UInt128* m_Powers = nullptr;		// Массив степеней (128 бит)
	HashTable<19> m_Hashes;					// Хеш-таблица значений степеней

private:
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

	// Главная функция рабочего потока
	void WorkerMainFn(Worker* worker);

	bool GetNextTask(Worker* worker);
	void OnTaskDone(Worker* worker);

	template<class NumberT>
	std::pair<unsigned, unsigned> CalcUpperValue(unsigned leftHigh = UINT_MAX) const;

	// Выполняет вычисления, начиная с указанных коэффициентов (задают первые коэффициенты стартового
	// задания), возвращает следующее за последним проверенным значение старшего коэффициента левой части
	unsigned Compute(const std::vector<unsigned>& startFactors);

	template<class NumberT>
	unsigned Compute(const std::vector<unsigned>& startFactors, unsigned toCheck);

	void ShowProgress(const unsigned* factors);

	void OnSolutionReady(const Solution& solution);
	void ProcessPendingSolutions();

	void UpdateActiveThreadCount();

private:
	thread::CriticalSection m_TaskCS;			// Критическая секция (задания)
	thread::CriticalSection m_ConsoleCS;		// Критическая секция (вывод в консоль)

	ThreadTimer m_MainThreadTimer;				// Счётчик времени CPU для главного потока
	std::vector<Worker*> m_Workers;				// Состояния рабочих потоков
	size_t m_ActiveWorkers = 0;					// Желаемое количество активных потоков

	Solutions m_Solutions;						// Найденные примитивные решения
	volatile size_t m_SolutionsFound = 0;		// Количество найденных примитивных решений
	std::vector<Solution> m_PendingSolutions;	// Найденные решения, ожидающие проверки

	Task* m_NextTask = nullptr;					// Следующее задание для выполнения
	unsigned m_LastHiFactor = 0;				// Последний старший коэффициент в блоке заданий
	unsigned m_LastDoneHiFactor = 0;			// Последний завершённый старший коэффициент

	std::vector<int> m_PendingTasks;			// Выполняемые и ожидающие проверки задания (ID потоков)
	volatile int m_LoPendingTask = 0;			// Самое младшее (старое) ожидаемое задание (ID потока)

	ProgressManager m_ProgressMan;				// Менеджер прогресса
	size_t m_LastProgressLength = 0;			// Количество символов в последнем выводе прогресса
	volatile bool m_ForceShowProgress = false;	// true, если прогресс нужно вывести немедленно
	volatile bool m_NeedUpdateTitle = false;	// true, если нужно обновить заголовок окна

	volatile bool m_NoTasks = false;			// true, если заданий для потоков больше нет
	volatile bool m_IsCancelled = false;		// true, если работа была прервана пользователем
	volatile bool m_IsAborted = false;			// true, если работа была прервана по иной причине
	volatile bool m_ForceQuit = false;			// true, если нужно прервать работу немедленно

	volatile bool m_PrintSolutions = false;		// true, если вывод решений на экран разрешён
	bool m_PrintAllSolutions = false;			// true, если указана опция "--printall"
};

//--------------------------------------------------------------------------------------------------------------------------------
struct FactorSearch::Task final
{
	// NB: рабочие потоки обычно не перебирают коэффициенты левой части уравнения (они все обычно
	// заданы заданием). Однако задание может задавать вместе со всеми коэффициентами левой части
	// и первые коэффициенты правой. Суммарное количество заданных коэффициентов в любом случае
	// не может превышать максимально возможного количества коэффициентов в левой части
	static constexpr int MAX_COUNT = MAX_FACTOR_COUNT / 2;

	int factorCount = 0;			// Количество заданных коэффициентов
	unsigned factors[MAX_COUNT];	// Заданные значения первых коэффициентов
	unsigned taskId = 0;			// Уникальный номер задания

	Task& operator =(const Task& that) noexcept;

	// Сравниваниет коэффициенты задания с коэффициентами решения.
	// Возвращает true, если задание "старше" указанного решения
	bool operator >(const Solution& rhs) const noexcept;
};

//--------------------------------------------------------------------------------------------------------------------------------
struct FactorSearch::Worker final
{
	int workerId = 0;					// Уникальный ID (от 1)
	std::thread threadObj;				// Объект потока
	ThreadTimer* timer = nullptr;		// Таймер затраченного потоком времени CPU

	Task task;							// Задание (первые коэффициенты)
	unsigned progressCounter = 0;		// Вспомогательный счётчик вывода прогресса

	volatile bool isActive = false;		// true, если поток выполняет вычисления
	volatile bool isFinished = false;	// true, если поток завершился (вышел из своей функции)
	volatile bool shouldPause = false;	// true, если поток должен приостановиться (не брать задание)
	volatile bool shouldQuit = false;	// true, если поток должен завершиться (после задания)
};
