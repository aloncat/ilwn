//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "forward.h"
#include "hashtable.h"
#include "powers.h"
#include "progressman.h"
#include "searchbase.h"
#include "solution.h"
#include "threadtimer.h"
#include "uint128.h"

#include <core/debug.h>
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

	// Выполняет поиск решений (начиная с указанных значений коэффициентов)
	virtual void Search(const Options& options, const std::vector<unsigned>& startFactors) override;

	// Обновляет заголовок окна консоли
	virtual void UpdateConsoleTitle() override;

	// Эта функция вызывается перед началом обработки блока заданий. Она должна
	// проинициализировать хеш-таблицу и установить функцию выполнения заданий
	virtual void BeforeCompute(unsigned upperLimit);

	// Эта функция вызывается сразу после завершения обработки блока заданий.
	// К этому моменту рабочие потоки находятся в приостановленном состоянии
	virtual void AfterCompute() {}

	// Возвращает значение между указанным стартовым значением старшего коэффициента левой части hiFactor
	// и желаемым конечным значением в блоке вычислений. Влияет на количество инициализируемых элементов
	// в массиве степеней и количество инициализируемых значений в хеш-таблице
	virtual unsigned GetChunkSize(unsigned hiFactor);

	// Инициализирует начальное задание task из набора коэфициентов startFactors
	// и устанавливает количество коэффициентов, которые будут задаваться заданием
	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors);

	// Выбирает следующее задание
	virtual void SelectNextTask(Task& task);

	// Функция рабочего потока, которая вызывается в цикле
	// до тех пор,  пока не будет установлен флаг shouldQuit
	virtual void WorkerFunction(Worker* worker);

	// Назначает указанному рабочему потоку следующее задание. Если задание было назначено, функция
	// вернёт true. Еси заданий больше нет (или работа была прервана), то функция вернёт false
	bool GetNextTask(Worker* worker);

	// Эта функция вызывается рабочим потоком после завершения им обработки задания
	void OnTaskDone(Worker* worker);

	// Функция вернёт true, если установлен флаг m_ForceQuit.
	// Вызывается рабочим потоком для вывода прогресса поиска
	bool OnProgress(const Worker* worker, const unsigned* factors);

	// Эта функция вызывается рабочим потоком при нахождении решения
	bool OnSolutionFound(const Worker* worker, const unsigned* factors);

	// Инициализирует указатель на функцию выполнения задания (значение m_SearchFn), выбирая подходящую
	// шаблонную функциию Derived::SearchFactors в классе наследнике в зависимости от типа чисел m_Powers
	template<class Derived>
	void SetSearchFn(Derived* searcher);

	// Инициализирует хеш-таблицу hashes значениями степеней от 1 до upperLimit
	template<size_t HASH_BITS>
	void InitHashTable(HashTable<HASH_BITS>& hashes, unsigned upperLimit);

	// Возвращает максимально возможное число потоков
	size_t GetMaxThreadCount() const { return m_Workers.size(); }

protected:
	// Максимальное количество выводимых решений
	static constexpr size_t MAX_SOLUTIONS = 100000;

	// Прототип функции, выполняющей задание в рабочем потоке
	using SearchFn = void (FactorSearch::*)(Worker* worker, const void* powers);

	// Прототип функции, проверяющей задание на отсутствие решений. Если для указанного
	// задания task гарантируется отсутствие решений, то функция должна вернуть false
	using CheckTaskFn = bool (*)(const Task& task);

	SearchFn m_SearchFn = nullptr;			// Функция выполнения задания
	CheckTaskFn m_CheckTaskFn = nullptr;	// Функция проверки отсутствия решений
	PowersBase* m_Powers = nullptr;			// Указатель на объект массива степеней
	HashTable<19> m_Hashes;					// Хеш-таблица значений степеней чисел

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
	void WorkerThreadFn(Worker* worker);

	template<class Derived, class NumberT>
	static SearchFn GetSearchFn();

	template<class NumberT>
	std::pair<unsigned, unsigned> CalcUpperValue(unsigned leftHigh = UINT_MAX) const;

	// Выполняет вычисления, начиная с указанных коэффициентов (задают первые коэффициенты стартового
	// задания), возвращает следующее за последним проверенным значение старшего коэффициента левой части
	unsigned Compute(const std::vector<unsigned>& startFactors);

	template<class NumberT>
	unsigned Compute(const std::vector<unsigned>& startFactors, unsigned toCheck);

	void ShowProgress(const unsigned* factors);
	void HideProgress();

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
	bool m_StopOnLimit = true;					// false, если указана опция --nolimit
};

//--------------------------------------------------------------------------------------------------------------------------------
template<class Derived>
void FactorSearch::SetSearchFn(Derived* searcher)
{
	m_SearchFn = nullptr;
	if (Verify(m_Powers && this == searcher))
	{
		if (m_Powers->IsType<uint64_t>())
			m_SearchFn = GetSearchFn<Derived, uint64_t>();
		else if (m_Powers->IsType<UInt128>())
			m_SearchFn = GetSearchFn<Derived, UInt128>();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<size_t HASH_BITS>
void FactorSearch::InitHashTable(HashTable<HASH_BITS>& hashes, unsigned upperLimit)
{
	Assert(m_Powers);

	if (m_Powers->IsType<uint64_t>())
	{
		auto powers = static_cast<Powers<uint64_t>*>(m_Powers);
		hashes.Init(upperLimit, *powers);
	}
	else if (m_Powers->IsType<UInt128>())
	{
		auto powers = static_cast<Powers<UInt128>*>(m_Powers);
		hashes.Init(upperLimit, *powers);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class Derived, class NumberT>
FactorSearch::SearchFn FactorSearch::GetSearchFn()
{
	class Helper : public Derived {
	public:
		using Fn = void (FactorSearch::*)(Worker*, const NumberT*);
		static Fn GetFn() { return static_cast<Fn>(&Helper::template SearchFactors); }
	};

	return reinterpret_cast<SearchFn>(Helper::GetFn());
}

//--------------------------------------------------------------------------------------------------------------------------------
struct FactorSearch::Task final
{
	// Рабочие потоки обычно не перебирают коэффициенты левой части уравнения (они все обычно
	// заданы заданием). Однако задание может задавать вместе со всеми коэффициентами левой части
	// и первые коэффициенты правой. Суммарное количество заданных коэффициентов в любом случае
	// не может превышать максимально возможного количества коэффициентов в левой части
	static constexpr int MAX_COUNT = MAX_FACTOR_COUNT / 2;

	int factorCount = 0;			// Количество заданных коэффициентов
	unsigned factors[MAX_COUNT];	// Заданные значения первых коэффициентов
	unsigned taskId = 0;			// Уникальный номер задания

	Task& operator =(const Task& that) noexcept;

	// Сравниваниет коэффициенты задания с коэффициентами указанного
	// решения. Возвращает true, если задание "старше" указанного решения
	bool operator >(const Solution& rhs) const noexcept { return rhs.IsLower(factors, factorCount);	}
};

//--------------------------------------------------------------------------------------------------------------------------------
struct FactorSearch::Worker final
{
	int workerId = 0;					// Уникальный ID (от 1)
	std::thread threadObj;				// Объект потока
	ThreadTimer* timer = nullptr;		// Таймер затраченного потоком времени CPU

	Task task;							// Задание (первые коэффициенты)
	unsigned progressCounter = 0;		// Вспомогательный счётчик вывода прогресса
	unsigned userData = 0;				// Свободная переменная для любых целей

	volatile bool isActive = false;		// true, если поток выполняет вычисления
	volatile bool isFinished = false;	// true, если поток завершился (вышел из своей функции)
	volatile bool shouldPause = false;	// true, если поток должен приостановиться (не брать задание)
	volatile bool shouldQuit = false;	// true, если поток должен завершиться (после задания)
};
