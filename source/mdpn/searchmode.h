//∙MDPN
#pragma once

#include "assert.h"
#include "dbmode.h"
#include "number.h"
#include "numset.h"

#include <core/platform.h>
#include <core/threadsync.h>
#include <core/util.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class SearchMode;
class ThreadTime;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные классы для режима работы SearchMode
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses
{
protected:
	struct NumberItem;
	enum class RAAType;
	struct NumberBlock;

	class Queue;
	class TaskQueue;
	class WorkQueue;
	class DBQueue;

	class WorkThreads;
};

//----------------------------------------------------------------------------------------------------------------------
struct SearchModeClasses::NumberItem
{
	uint32_t stepDoneC = 0;		// Кол-во операций RAA, выполненных над числом num (31-й бит - признак палиндрома)
	uint16_t siftLength = 0;	// Целевая длина числа num для проверки на отсев, она же длина числа sifting
	uint16_t stepLimit = 0;		// Ограничение на количество шагов при проверке числа num на палиндром

	FixNumber num;				// Исходное проверяемое число (кандидат)
	FixNumber sifting;			// Результат операций RAA над num после достижения длины siftLength

	void Clear();
	// Возвращает true, если число было обработано, то есть если
	// изначально это был не "пустой" элемент массива чисел (блока)
	bool IsValid() const { return stepDoneC != 0; }
	// Возвращает true, если после обработки число num стало
	// палиндромом; функция вернёт false, если это число Лишрел
	bool IsPalindrome() const { return (stepDoneC & 0x80000000) != 0; }
	// Возвращает количество операций RAA, выполненных над исходным числом
	unsigned GetStepDoneC() const { return stepDoneC & 0x7fffffff; }
};

//----------------------------------------------------------------------------------------------------------------------
struct SearchModeClasses::NumberBlock
{
	// Размер блока чисел для обработки в качестве отдельного задания. Выбран таким образом, чтобы
	// худшее время обработки для всего блока (без отсева чисел Лишрел) было равно в среднем ~15 ms
	static constexpr size_t SIZE = 2560;

	uint64_t id = 0;			// Порядковый номер блока
	uint64_t cpuTime = 0;		// Суммарное время (микросекунды), затраченное потоками на обработку блока
	Number lastNum;				// Последнее проверяемое число (кандидат) для блока
	NumberItem numA[SIZE];		// Массив чисел для обработки
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::*Queue - очереди заданий для взаимодействия потоков
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses::Queue
{
	AML_NONCOPYABLE(Queue)

protected:
	static constexpr size_t DEFAULT_SPIN_C = 100;

	class MutexLock : public std::unique_lock<std::mutex> {
	public:
		MutexLock(Queue* pThis, size_t spinCount = DEFAULT_SPIN_C)
			: unique_lock(pThis->LockMutex(spinCount), std::adopt_lock)
		{
		}
	};

	Queue() = default;
	~Queue();

	// Захватывает мьютекс m_Mutex. Если мьютекс занят, то сначала будут сделаны spinCount
	// попыток захватить его в цикле (SpinLock) и лишь затем последует блокировка потока
	std::mutex& LockMutex(size_t spinCount = DEFAULT_SPIN_C);

	std::mutex m_Mutex;
	std::deque<NumberBlock*> m_Queue;
};

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses::TaskQueue final : Queue
{
public:
	TaskQueue(WorkThreads& workThreads);

	void PushTask(NumberBlock* pBlock);
	NumberBlock* PopTask(bool waitIfNoTask);

	size_t GetTaskC() const { return m_TaskC.load(std::memory_order_relaxed); }
	void WakeThread(bool all = false);

private:
	WorkThreads& m_WorkThreads;
	std::condition_variable m_CV;
	std::atomic<size_t> m_TaskC = 0;
};

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses::WorkQueue : protected Queue
{
public:
	void PushWork(NumberBlock* pBlock);
	NumberBlock* PopWork();

	bool HasWorks() const { return m_HasWorks.load(std::memory_order_relaxed); }

protected:
	std::atomic<bool> m_HasWorks = false;
};

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses::DBQueue final : public WorkQueue
{
public:
	~DBQueue();

	void PushTask(NumberBlock* pBlock, unsigned stepLimit);
	NumberBlock* PopTask(unsigned& stepLimit);

	void WakeThread();

private:
	struct Item {
		NumberBlock* pBlock;
		unsigned stepLimit;
	};

	std::deque<Item> m_Tasks;
	std::condition_variable m_CV;
	std::atomic<bool> m_HasTasks = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::WorkThreads
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SearchModeClasses::WorkThreads final : AssertHelper<>
{
	AML_NONCOPYABLE(WorkThreads)

public:
	// Функция потока. Должна возвращать false, если для потока в
	// данный момент нет заданий и он может быть деактивирован
	using ThreadFn = std::function<bool(ThreadTime& timer)>;

	WorkThreads(SearchMode* pOwner);
	~WorkThreads();

	// Возвращает количество рабочих потоков. Активными из них могут быть не все. Но
	// если это значение отлично от 0, то как минимум 1 поток всегда будет активен
	size_t GetThreadC() const { return m_TotalThreadC; }
	// Возвращает количество активных в данный момент потоков
	size_t GetActiveC() const { return m_ActiveC.load(std::memory_order_relaxed); }

	// Изменяет количество рабочих потоков. Параметр count задаёт число новых потоков,
	// которые необходимо создать (+), или число потоков, которые нужно остановить (-)
	void AddRemove(int count);

	void CreateAll(const ThreadFn& threadFn);
	void KillAll();

private:
	// Максимально возможное количество рабочих потоков. Теоритически может быть любым значением,
	// реальное количество потоков будет ограничено количеством логических процессоров в системе
	static constexpr size_t MAX_THREAD_C = 22;

	struct ThreadInfo {
		std::thread threadObj;				// Объект потока
		unsigned lowLoadCounter = 0;		// Счётчик интервалов с низкой загрузкой
		uint32_t lastCPULoadTick = 0;		// Тик последней оценки загруженности потока
		uint64_t lastCPULoadCounter = 0;	// Значение счётчика последней оценки загруженности
		volatile bool isActive = false;		// true, если поток активен (false, если приостановлен)
		volatile bool isStopping = false;	// true, если поток должен завершить работу
	};

	static uint64_t GetPerfCounter();
	static uint64_t GetPerfCounter(uint64_t& frequency);
	static void GetCoreC(size_t& physicalCoreC, size_t& logicalCoreC);

	void DoThread(size_t index);
	void CheckThreadLoad(size_t index, ThreadTime& threadTime, bool hadWork);
	bool AreThreadsOverloaded();
	void WakeOneThread();

	SearchMode& m_Owner;
	thread::CriticalSection m_CS;

	ThreadFn m_ThreadFn;					// Пользовательская функция рабочих потоков
	size_t m_MaxThreadC = 0;				// Максимально возможное количество рабочих потоков
	volatile size_t m_TotalThreadC = 0;		// Количество созданных (актуальных) рабочих потоков
	ThreadInfo m_ThreadA[MAX_THREAD_C];		// Рабочие потоки (актуальные от [0] до [m_TotalThreadC - 1])
	uint64_t m_ThreadTimeA[MAX_THREAD_C];	// Значения времени CPU потоков в последней оценке загруженности
	std::atomic<size_t> m_ActiveC = 0;		// Количество активных рабочих потоков в данных момент
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchMode - поиск отложенных палиндромов (основной режим работы программы)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SearchMode final : public DBMode, SearchModeClasses, AssertHelper<>
{
public:
	SearchMode();
	virtual ~SearchMode() override;

	virtual bool Run() override;
	bool IsCancelled() const { return m_IsCancelled; }

private:
	struct Progress {
		uint64_t counter = 0;		// Количество проверенных чисел с момента последнего вывода прогресса
		uint64_t progress = 0;		// Полное количество проверенных чисел в текущем диапазоне
		unsigned totalSeconds = 0;	// Общее время работы в секундах (накопленное значение)
		uint32_t startTime = 0;		// Тик времени в момент начала работы (периодически обновляется)
		uint32_t lastTick = 0;		// Тик, в котором прогресс выводился на экран в последний раз
		float lastSpeed = 0;		// Последнее вычисленное значение скорости проверки чисел
	};

	void CreateThreads();
	void KillThreads();

	bool SlowSearch(bool createNewDb, const Number& startFrom = 1u);
	void DoSearch(const Number& firstNum);

	bool OnRangeCompleted();
	void UpdateStepLimit(unsigned& stepLimit, const Number& number);
	void PrintProgress(uint32_t tick, const Number& lastNum);
	bool UpdateProgress(const Number& lastNum);
	void CreateNewChunk(const Number& first);
	void SaveResults();

	NumberBlock* GetNumberBlock();
	void ReleaseSurplusNumberBlocks(size_t count);

	void ProcessWork(NumberBlock* pWork, unsigned stepLimit);
	bool ProcessDBWork(NumberBlock* pWork);
	bool DoNextTask(ThreadTime& threadTime, bool waitIfNoTask);
	void DBThreadFN();

	WorkThreads m_WorkThreads;					// Рабочие потоки
	std::thread* m_pDBThread = nullptr;			// Поток базы данных

	std::vector<NumberBlock*> m_NumBlocks;		// Свободные блоки чисел
	TaskQueue m_Tasks;							// Очередь FIFO для заданий
	WorkQueue m_Works;							// Очередь FIFO для результатов
	DBQueue m_DBQueue;							// Очередь заданий сохранения результатов в БД

	NumberSet m_SiftSet;						// Набор отсева чисел Лишрел
	std::atomic<uint32_t> m_SiftSetReaderC = 0;	// Счётчик "читателей" набора m_SiftSet; 31-й бит == <можно читать>
	std::condition_variable m_SiftSetCV;		// CV пробуждения главного потока при ожидании им записи в m_SiftSet
	std::mutex m_SiftSetMutex;					// Мьютекс для m_SiftSetCV

	thread::CriticalSection m_DBCS;				// Крит. секция для синхронизации с потоком БД
	DBChunk* volatile m_pActiveChunk = nullptr;	// Текущий (активный) файл БД
	volatile uint32_t m_LastSaveTick = 0;		// Тик последнего сохранения данных в файл БД
	volatile uint64_t m_CPUTime = 0;			// Затраченное на проверку время CPU (в ms)
	Number m_Last;								// Последнее проверенное число

	Progress m_Progress;						// Параметры для отслеживания прогресса проверки чисел

	bool m_IsExecuted = false;					// true, если функция Run была вызвана
	volatile bool m_IsCancelled = false;		// true, если пользователь отменил операцию
	volatile bool m_StopDBThread = false;		// если true, то поток базы данных должен прекратить работу
	std::atomic<bool> m_PublishEvents = false;	// true, если нужно вывести накопленные сообщения о событиях
};
