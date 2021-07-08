//∙MDPN
#include "pch.h"
#include "searchmode.h"

#include "const.h"
#include "dbchunk.h"
#include "eventmgr.h"
#include "log.h"
#include "ttime.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/console.h>
#include <core/strutil.h>
#include <core/winapi.h>

#include <intrin.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::NumberItem
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::NumberItem::Clear()
{
	stepDoneC = 0;
	siftLength = 0;
	stepLimit = 0;

	num.SetZero();
	sifting.SetZero();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::Queue
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::Queue::~Queue()
{
	for (NumberBlock* pBlock : m_Queue)
		AML_SAFE_DELETE(pBlock);
}

//----------------------------------------------------------------------------------------------------------------------
std::mutex& SearchModeClasses::Queue::LockMutex(size_t spinCount)
{
	while (!m_Mutex.try_lock())
	{
		if (spinCount--)
			_mm_pause();
		else
		{
			m_Mutex.lock();
			break;
		}
	}
	return m_Mutex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::TaskQueue
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::TaskQueue::TaskQueue(WorkThreads& workThreads)
	: m_WorkThreads(workThreads)
{
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::TaskQueue::PushTask(NumberBlock* pBlock)
{
	if (pBlock)
	{
		size_t taskC;
		{
			MutexLock lock(this);

			m_Queue.push_back(pBlock);
			// NB: внутри критической секции атомарность инкремента
			// не нужна, поэтому избегаем лишних блокировок шины
			taskC = m_TaskC.load(std::memory_order_relaxed);
			m_TaskC.store(taskC + 1, std::memory_order_relaxed);
		}

		// Если в очереди накопилось 10 и более заданий в расчёте на каждый активный поток,
		// то разбудим один из них сразу. В ином случае поток будет разбужен по таймеру
		if (taskC >= 10 * m_WorkThreads.GetActiveC())
			m_CV.notify_one();
	}
}

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::NumberBlock* SearchModeClasses::TaskQueue::PopTask(bool waitIfNoTask)
{
	MutexLock lock(this);

	if (m_Queue.empty())
	{
		if (!waitIfNoTask)
			return nullptr;

		m_CV.wait_for(lock, std::chrono::milliseconds(35));
		if (m_Queue.empty())
			return nullptr;
	}

	NumberBlock* pBlock = m_Queue.front();
	// NB: внутри критической секции атомарность декремента
	// не нужна, поэтому избегаем лишних блокировок шины
	const size_t taskC = m_TaskC.load(std::memory_order_relaxed);
	m_TaskC.store(taskC - 1, std::memory_order_relaxed);
	m_Queue.pop_front();
	return pBlock;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::TaskQueue::WakeThread(bool all)
{
	if (GetTaskC())
	{
		if (all)
			m_CV.notify_all();
		else
			m_CV.notify_one();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::WorkQueue
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkQueue::PushWork(NumberBlock* pBlock)
{
	if (pBlock)
	{
		MutexLock lock(this);

		m_Queue.push_back(pBlock);
		m_HasWorks.store(true, std::memory_order_relaxed);
	}
}

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::NumberBlock* SearchModeClasses::WorkQueue::PopWork()
{
	if (m_HasWorks.load(std::memory_order_relaxed))
	{
		MutexLock lock(this);

		if (size_t count = m_Queue.size())
		{
			NumberBlock* pBlock = m_Queue.front();
			m_HasWorks.store(count > 1, std::memory_order_relaxed);
			m_Queue.pop_front();
			return pBlock;
		}
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::DBQueue
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::DBQueue::~DBQueue()
{
	for (Item& item : m_Tasks)
		AML_SAFE_DELETE(item.pBlock);
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::DBQueue::PushTask(NumberBlock* pBlock, unsigned stepLimit)
{
	if (pBlock)
	{
		size_t taskCount;
		{
			MutexLock lock(this);

			m_Tasks.push_back(Item { pBlock, stepLimit });
			m_HasTasks.store(true, std::memory_order_relaxed);
			taskCount = m_Tasks.size();
		}

		// Если в очереди накопилось 8 и более заданий, то разбудим поток
		// БД сразу. В ином случае поток будет разбужен по таймеру
		if (taskCount >= 8)
			m_CV.notify_one();
	}
}

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::NumberBlock* SearchModeClasses::DBQueue::PopTask(unsigned& stepLimit)
{
	MutexLock lock(this);

	if (m_Tasks.empty())
	{
		m_CV.wait_for(lock, std::chrono::milliseconds(35));
		if (m_Tasks.empty())
		{
			stepLimit = 0;
			return nullptr;
		}
	}

	Item& item = m_Tasks.front();
	m_HasTasks.store(m_Tasks.size() > 1, std::memory_order_relaxed);
	NumberBlock* pBlock = item.pBlock;
	stepLimit = item.stepLimit;
	m_Tasks.pop_front();
	return pBlock;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::DBQueue::WakeThread()
{
	if (m_HasTasks.load(std::memory_order_relaxed))
		m_CV.notify_one();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchModeClasses::WorkThreads
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::WorkThreads::WorkThreads(SearchMode* pOwner)
	: m_Owner(*pOwner)
{
	Assert(pOwner);
}

//----------------------------------------------------------------------------------------------------------------------
SearchModeClasses::WorkThreads::~WorkThreads()
{
	KillAll();
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::AddRemove(int count)
{
	if (count > 0)
	{
		Assert(m_ThreadFn && m_MaxThreadC);

		size_t newC = m_TotalThreadC + count;
		newC = std::min(newC, m_MaxThreadC);

		if (newC > m_TotalThreadC)
		{
			thread::CriticalSection::Lock lock(m_CS);
			for (size_t i = m_TotalThreadC; i < newC; ++i)
			{
				m_ThreadTimeA[i] = 0;
				ThreadInfo& info = m_ThreadA[i];

				info.isActive = false;
				info.isStopping = false;
				info.threadObj = std::thread([=]() { DoThread(i); });
				++m_TotalThreadC;
			}
		}
	}
	else if (count < 0)
	{
		size_t toStopC = -count;
		if (toStopC > m_TotalThreadC)
			toStopC = m_TotalThreadC;

		if (toStopC)
		{
			thread::CriticalSection::Lock lock(m_CS);
			for (size_t i = 0; i < toStopC; ++i)
			{
				ThreadInfo& info = m_ThreadA[m_TotalThreadC - i - 1];
				info.isStopping = true;
			}
			for (size_t i = 0; i < toStopC; ++i)
			{
				ThreadInfo& info = m_ThreadA[--m_TotalThreadC];
				if (info.threadObj.joinable())
				{
					// Для избежания дедлока в ситуациях, когда рабочий поток был переключен диспетчером ОС,
					// например, между проверкой им фалага info.isStopping и установкой info.isActive или
					// между установкой этого же флага и вызовом SuspendThread, мы будем ждать завершения
					// потока в цикле, периодически проверяя по тайм-ауту, не был ли он приостановлен
					while (void* pOSHandle = info.threadObj.native_handle())
					{
						if (!info.isActive)
							::ResumeThread(pOSHandle);
						if (::WaitForSingleObject(pOSHandle, 1) != WAIT_TIMEOUT)
							break;
					}
					// Окончательно завершаем поток
					info.threadObj.join();
				}
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::CreateAll(const ThreadFn& threadFn)
{
	// Функция CreateAll должна вызываться в самом начале работы. Поэтому
	// пользовательская функция потоков и их количество должны быть не заданы
	Assert(!m_ThreadFn && !m_TotalThreadC && threadFn);

	size_t physicalC, logicalC;
	GetCoreC(physicalC, logicalC);
	// Если количество физических и логических ядер процессора одинаково (т.е. CPU без HT), то рабочих
	// потоков должно быть на 1 меньше, чем ядер, а главный поток полностью загрузит оставшееся ядро.
	// Если CPU с HT, то обычно логических ядер будет вдвое больше. В таком случае будем использовать
	// для рабочих потоков все логические ядра, кроме 2, которые займут главный поток и поток БД
	const size_t threadC = (physicalC < logicalC) ? logicalC - 2 : physicalC - 1;
	m_MaxThreadC = (threadC < MAX_THREAD_C) ? threadC : MAX_THREAD_C;

	m_ThreadFn = threadFn;
	AddRemove(static_cast<int>(m_MaxThreadC));
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::KillAll()
{
	AddRemove(-static_cast<int>(MAX_THREAD_C));
	m_ThreadFn = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t SearchModeClasses::WorkThreads::GetPerfCounter()
{
	LARGE_INTEGER t;
	::QueryPerformanceCounter(&t);
	return t.QuadPart;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t SearchModeClasses::WorkThreads::GetPerfCounter(uint64_t& frequency)
{
	LARGE_INTEGER t, f;
	::QueryPerformanceCounter(&t);
	::QueryPerformanceFrequency(&f);
	frequency = std::max(f.QuadPart, 1ll);
	return t.QuadPart;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::GetCoreC(size_t& physicalCoreC, size_t& logicalCoreC)
{
	SYSTEM_INFO sysInfo;
	::GetSystemInfo(&sysInfo);
	logicalCoreC = sysInfo.dwNumberOfProcessors ? sysInfo.dwNumberOfProcessors : 1;
	physicalCoreC = logicalCoreC;

	DWORD bufferSize = 0;
	::GetLogicalProcessorInformation(nullptr, &bufferSize);
	if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER && bufferSize)
	{
		void* p = new uint8_t[bufferSize];
		auto pInfo = static_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(p);
		if (::GetLogicalProcessorInformation(pInfo, &bufferSize))
		{
			size_t coreC = 0, logicalC = 0;
			for (size_t size = 0; size < bufferSize; size += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))
			{
				if (pInfo->Relationship == RelationProcessorCore)
				{
					++coreC;
					for (auto mask = pInfo->ProcessorMask; mask; mask >>= 1)
						logicalC += (mask & 1) ? 1 : 0;
				}
				++pInfo;
			}
			physicalCoreC = coreC ? coreC : 1;
			logicalCoreC = logicalC ? logicalC : 1;
			if (logicalCoreC < physicalCoreC)
				logicalCoreC = physicalCoreC;
		}
		delete[] p;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::DoThread(size_t index)
{
	ThreadInfo& info = m_ThreadA[index];
	info.isActive = true;
	++m_ActiveC;

	// Рабочий поток будет выполняться с наименьшим приоритетом
	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_IDLE);

	info.lowLoadCounter = 0;
	info.lastCPULoadTick = ::GetTickCount();
	info.lastCPULoadCounter = GetPerfCounter();

	ThreadTime threadTime, timer;
	while (!info.isStopping && !m_Owner.IsCancelled())
	{
		bool hadWork = m_ThreadFn && m_ThreadFn(timer);
		CheckThreadLoad(index, threadTime, hadWork);
	}

	info.isActive = false;
	--m_ActiveC;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::CheckThreadLoad(size_t index, ThreadTime& threadTime, bool hadWork)
{
	ThreadInfo& info = m_ThreadA[index];

	if (!index)
	{
		// Самый первый ребочий поток никогда не приостанавливается. Вместо этого мы будем в
		// каждом интервале (независимо от наличия у нашего потока работы в последнем цикле)
		// проверять, не требуется ли нам разбудить другие приостановленные потоки
		const uint32_t tick = ::GetTickCount();
		if (tick - info.lastCPULoadTick >= 500 && m_CS.TryEnter())
		{
			info.lastCPULoadTick = tick;
			// Критическая секция захвачена (если бы нам не удалось её захватить, то мы бы пропустили
			// проверку). Мы не должны выполнять проверку во время уничтожения/создания потоков в AddRemove
			thread::CriticalSection::Lock lock(m_CS, false);
			if (AreThreadsOverloaded())
				WakeOneThread();
		}
	}
	else if (!hadWork)
	{
		// Для всех рабочих потоков кроме самого первого мы будем измерять загруженность,
		// причём будем делать это, только если в последнем цикле у потока не было работы
		const uint32_t tick = ::GetTickCount();
		if (tick - info.lastCPULoadTick >= 500)
		{
			info.lastCPULoadTick = tick;

			uint64_t freq, counter = GetPerfCounter(freq);
			uint64_t ticksElapsed = counter - info.lastCPULoadCounter;
			info.lastCPULoadCounter = counter;

			// Вычисляем загрузку потока в %. Несмотря на то, что мы используем точное значение прошедшего
			// времени, значение загрузки будет иметь погрешность до ~6% (при интервале опроса в 500ms)
			// из-за того, что данные о времени потока обновляются системой с интервалом ~15ms
			float cpuLoad = threadTime.GetElapsed(true) / (10000.f * ticksElapsed / freq);

			constexpr size_t GAIN_C = 7;
			size_t activeThreadC = GetActiveC();
			activeThreadC = (activeThreadC < GAIN_C) ? activeThreadC : GAIN_C - 1;
			static const int loGainA[GAIN_C] = { 0, 0, 40, 53, 60, 64, 66 };
			static const int hiGainA[GAIN_C] = { 0, 0, 60, 75, 80, 80, 80 };

			if (cpuLoad < loGainA[activeThreadC])
			{
				++info.lowLoadCounter;
				if (info.lowLoadCounter >= 8 && !info.isStopping)
				{
					--m_ActiveC;
					info.isActive = false;
					::SuspendThread(::GetCurrentThread());
					info.isActive = true;
					++m_ActiveC;

					info.lowLoadCounter = 0;
					info.lastCPULoadTick = ::GetTickCount();
					info.lastCPULoadCounter = GetPerfCounter();
				}
			}
			else if (cpuLoad >= hiGainA[activeThreadC])
				info.lowLoadCounter = 0;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchModeClasses::WorkThreads::AreThreadsOverloaded()
{
	uint64_t totalCPUTime = 0;
	for (size_t i = 0; i < m_TotalThreadC; ++i)
	{
		FILETIME t1, t2, tk, tu;
		void* pOSHandle = m_ThreadA[i].threadObj.native_handle();
		if (pOSHandle && ::GetThreadTimes(pOSHandle, &t1, &t2, &tk, &tu))
		{
			uint64_t cpuTime = (static_cast<uint64_t>(tk.dwHighDateTime) << 32) | tk.dwLowDateTime;
			cpuTime += (static_cast<uint64_t>(tu.dwHighDateTime) << 32) | tu.dwLowDateTime;
			totalCPUTime += cpuTime - m_ThreadTimeA[i];
			m_ThreadTimeA[i] = cpuTime;
		}
	}

	uint64_t freq, counter = GetPerfCounter(freq);
	uint64_t ticksElapsed = counter - m_ThreadA[0].lastCPULoadCounter;
	m_ThreadA[0].lastCPULoadCounter = counter;

	// Вычисляем среднюю загрузку активных потоков в процентах. Считаем, что потоки
	// перегружены, если средняя их загрузка в последнем интервале была не менее 85%
	float cpuLoad = totalCPUTime / (100000.f * GetActiveC() * ticksElapsed / freq);
	return cpuLoad >= 85;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchModeClasses::WorkThreads::WakeOneThread()
{
	if (GetActiveC() < m_TotalThreadC)
	{
		for (size_t i = 1; i < m_TotalThreadC; ++i)
		{
			ThreadInfo& info = m_ThreadA[i];
			if (!info.isActive && !info.isStopping)
			{
				if (void* pOSHandle = info.threadObj.native_handle())
					::ResumeThread(pOSHandle);
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchMode
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
SearchMode::SearchMode()
	: m_WorkThreads(this)
	, m_Tasks(m_WorkThreads)
	, m_SiftSet(true)
	, m_DBCS(300)
{
}

//----------------------------------------------------------------------------------------------------------------------
SearchMode::~SearchMode()
{
	KillThreads();
	for (auto& pBlock : m_NumBlocks)
		AML_SAFE_DELETE(pBlock);
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::Run()
{
	if (!m_IsExecuted)
	{
		m_IsExecuted = true;

		// Без параметров - продолжаем поиск чисел, начиная с самого последнего проверенного
		// числа в базе данных. Если БД не существует (не найдена), то завершаемся с ошибкой
		if (m_Params.empty())
			return SlowSearch(false);

		// Команда "new" - создание новой базы данных. Второй опциональный параметр - число,
		// с которого начнётся поиск. Если БД уже существует, то программа завершится с ошибкой
		if (m_Params.size() >= 1 && m_Params.size() <= 2 && !util::StrInsCmp(m_Params[0], "new"))
		{
			if (m_Params.size() == 1)
				return SlowSearch(true);

			if (IsNumber(m_Params[1].c_str()))
			{
				const Number first = m_Params[1];
				if (first && first.GetLength() <= Const::MAX_DIGIT_C - 2)
					return SlowSearch(true, first);
			}
		}

		m_IsExecuted = false;
		OnInvalidCmdLine();
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::CreateThreads()
{
	Assert(!m_WorkThreads.GetThreadC() && !m_pDBThread);

	m_WorkThreads.CreateAll([this](ThreadTime& timer) { return DoNextTask(timer, true); });
	m_pDBThread = new std::thread([this]() { DBThreadFN(); });
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::KillThreads()
{
	m_WorkThreads.KillAll();

	if (m_pDBThread)
	{
		m_StopDBThread = true;
		if (m_pDBThread->joinable())
			m_pDBThread->join();
		AML_SAFE_DELETE(m_pDBThread);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::SlowSearch(bool createNewDb, const Number& startFrom)
{
	m_Progress.startTime = ::GetTickCount();
	if (!m_Data.Init(createNewDb, DBChunkState::HEADERONLY))
	{
		aux::Print(createNewDb ? "Failed to create new database, exiting...\n" :
			"Failed to load existing database, exiting...\n");
		return false;
	}

	Number firstNum;
	std::string initMsg;

	if (createNewDb)
	{
		firstNum = startFrom;
		initMsg = (firstNum <= Number(1u)) ? "Database has been created, starting from scratch..." :
			util::Format("Starting new database from number #15#%s#7...", SeparateWithCommas(firstNum).c_str());
	} else
	{
		firstNum = m_Data.GetLast() + 1u;
		if (firstNum.GetLength() > Const::MAX_DIGIT_C)
			return false;
		initMsg = util::Format("Database loaded, continuing from #15#%s#7...",
			SeparateWithCommas(firstNum).c_str());
	}

	SystemLog::SetPath(m_Data.GetBasePath() + L"log.txt");
	PrintDataBasePath(m_Data.GetBasePath(), 46);
	EventManager::PublishEvent(initMsg);

	uint64_t progress = 0;
	const size_t curRange = firstNum.GetLength();
	m_Data.ForEachChunk([curRange, &progress](DBChunk* pChunk) {
		if (pChunk->GetLast().GetLength() == curRange)
			progress += pChunk->GetIterationC();
		pChunk->UnloadData(DBChunkState::DATAUNLOADED);
		return true;
	});

	size_t startRange = m_Data.HasFound(1) ? 1 : curRange;
	m_Steps = std::make_unique<StepHelper>(startRange);
	m_Events = std::make_unique<EventManager>(m_Data);

	CreateThreads();
	m_Progress.progress = progress;
	m_Progress.lastTick = ::GetTickCount();
	m_LastSaveTick = m_Progress.lastTick;
	DoSearch(firstNum);
	KillThreads();

	const uint32_t endTime = ::GetTickCount();
	const float timeInWork = m_Progress.totalSeconds + .001f * (endTime - m_Progress.startTime);

	SystemLog::Instance().Close();
	aux::Printf("Time in work: %.1fs\n", timeInWork);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::DoSearch(const Number& firstNum)
{
	Assert(firstNum && firstNum.GetLength() <= Const::MAX_DIGIT_C);
	m_SiftSetReaderC.store(1u << 31, std::memory_order_relaxed);

	if (m_Data.HasGaps())
	{
		EventManager::PublishEvent(util::Format("#12WARNING: #10#%u#3-digit range"
			" has one or more unsearched gaps!", firstNum.GetLength()));
	}

	if (!m_SiftSet.IsLargePageEnabled())
	{
		EventManager::PublishEvent("#12WARNING: #3Large page support is not enabled!");
	}

	unsigned stepLimit = m_Steps->GetSearchLimit(firstNum);
	EventManager::PublishEvent(util::Format("#6Current search depth was set to #12#%u#6 steps", stepLimit));
	PrintProgress(::GetTickCount(), firstNum);

	ThreadTime threadTime;
	CreateNewChunk(firstNum);
	BigNumber lastNum = firstNum - 1u;
	size_t lastNumLength = firstNum.GetLength();

	size_t conseqLen = 20;
	if (lastNumLength + 4 > conseqLen)
	{
		// Длина числа, до достижения которой над ним выполняются операции RAA, прежде чем оно будет проверено
		// на сходимость к одному из потоков уже проверенных чисел Лишрел (и добавлено в набор). Тестирование
		// показало, что длины на 4 большей, чем длина тестируемых чисел, достаточно для эффективного отсева,
		// а чем ниже будет это значение, тем быстрее будет происходить отсев. Значение не должно превышать
		// максимальную длину чисел, хранимых в NumberSet (для FixNumber сейчас это 30 знаков)
		conseqLen = std::min(lastNumLength + 4, Const::MAX_DIGIT_C);
	}

	uint64_t nextNewBlockId = 0, nextReadyBlockId = 0;
	size_t pendingTaskC = 0, pendingDBTaskC = 0;
	std::vector<NumberBlock*> pendingWorks;
	uint32_t lastTick = ::GetTickCount();
	bool wait, checkPending = false;
	bool rangeCompleted = false;

	while (true)
	{
		// Выставим флаг wait. Если в теле цикла будет сделана какая-нибудь полезная работа,
		// то мы сбросим этот флаг. Но если к концу цикла флаг всё ещё будет установлен, то
		// попробуем выполнить одно задание, предназначенное для рабочих потоков
		wait = true;

		// Первым делом проверим, есть ли задания в очереди готовых заданий и извлечём из неё
		// самое первое (если есть). Если очередь пуста, то PopWork сразу вернёт управление
		NumberBlock* pWork = m_Works.PopWork();
		if (pWork)
		{
			--pendingTaskC;
			// Если id задания не тот, которого мы ожидаем следующим, то
			// поместим его в очередь ожидания, чтобы обработать позднее
			if (pWork->id != nextReadyBlockId)
			{
				pendingWorks.push_back(pWork);
				pWork = nullptr;
			}
		}

		// Если новых готовых заданий нет, то проверим ожидающие обработки готовые задания, которые
		// были получены в неверном порядке (раньше, чем те, которые должны быть обработаны первыми)
		if (!pWork && checkPending)
		{
			// NB: используем линейный поиск в несортированном массиве, так как это быстрее всего.
			// Среднее значение длины вектора будет близко к количеству рабочих потоков, а искомый
			// элемент, если он присутствует в векторе, будет чаще находиться ближе к его началу
			for (auto it = pendingWorks.begin(); it != pendingWorks.end(); ++it)
			{
				if ((*it)->id == nextReadyBlockId)
				{
					pWork = *it;
					pendingWorks.erase(it);
					break;
				}
			}
			checkPending = false;
		}

		// Если следующее по позрастанию id задание готово,
		// то обработаем его и передадим дальше потоку БД
		if (pWork)
		{
			ProcessWork(pWork, stepLimit);
			pWork->cpuTime += threadTime.GetElapsed(true);
			m_DBQueue.PushTask(pWork, stepLimit);
			checkPending = !pendingWorks.empty();
			++nextReadyBlockId;
			++pendingDBTaskC;
			wait = false;
		}

		// Теперь проверим очередь готовых заданий от потока БД и обработаем за раз
		// все её задания. Если очередь пуста, то PopWork сразу вернёт управление
		while (NumberBlock* pDBWork = m_DBQueue.PopWork())
		{
			--pendingDBTaskC;
			m_IsCancelled = !ProcessDBWork(pDBWork) || !UpdateProgress(pDBWork->lastNum);
			m_NumBlocks.push_back(pDBWork);
			if (m_IsCancelled)
				break;
		}
		if (m_IsCancelled)
			break;

		// Проверим таймер. Если прошло достаточно времени, попытаемся разбудить спящие рабочие потоки и
		// поток БД (если потоки спят и в соответствующих очередях есть задания, то они будут разбужены)
		const uint32_t tick = ::GetTickCount();
		if (tick - lastTick >= 10)
		{
			lastTick = tick;
			m_Tasks.WakeThread();
			m_DBQueue.WakeThread();
		}

		// Если текущий диапазон был закончен, то ждём готовности оставшихся заданий
		if (rangeCompleted && !(pendingTaskC + pendingWorks.size()) && !pendingDBTaskC)
		{
			rangeCompleted = false;
			if (!OnRangeCompleted())
				break;
			Number next = lastNum + 1u;
			lastNumLength = next.GetLength();
			if (lastNumLength + 4 > conseqLen)
			{
				conseqLen = std::min(lastNumLength + 4, Const::MAX_DIGIT_C);
				m_SiftSet.Clear(false);
			}
			UpdateStepLimit(stepLimit, next);
		}

		const size_t totalPendingC = pendingTaskC + pendingWorks.size();
		const size_t threadC = std::max(m_WorkThreads.GetThreadC(), size_t(1));
		// Если в очереди рабочих потоков недостаточно заданий, добавим ещё, но при условии, что в
		// очередях готовых заданий и очереди БД нет большого количества скопившихся готовых заданий
		if (!rangeCompleted && m_Tasks.GetTaskC() < 32 * threadC && totalPendingC < 192 && pendingDBTaskC < 128)
		{
			NumberBlock* pNumBlock = GetNumberBlock();
			pNumBlock->id = nextNewBlockId++;
			pNumBlock->cpuTime = 0;

			for (size_t i = 0; i < NumberBlock::SIZE; ++i)
			{
				++lastNum;
				lastNum.SkipRAADups();

				if (lastNum.GetLength() > lastNumLength)
				{
					for (size_t j = i; j < NumberBlock::SIZE; ++j)
						pNumBlock->numA[j].Clear();
					rangeCompleted = true;
					--lastNum;
					break;
				}

				auto& item = pNumBlock->numA[i];
				item.stepDoneC = 0;
				item.siftLength = static_cast<uint16_t>(conseqLen);
				item.stepLimit = static_cast<uint16_t>(stepLimit);
				item.num = lastNum;
			}

			pNumBlock->lastNum = lastNum;
			m_Tasks.PushTask(pNumBlock);
			++pendingTaskC;
			wait = false;
		}

		// Если мы ничего полезного не сделали (для диспетчера нет работы), и очередь готовых заданий пуста,
		// то выполним следующее задание. Если заданий нет, то отдадим остаток тайм-слайса системе. Помимо
		// задания будем освобождать каждый раз небольшое количество неиспользуемых блоков чисел
		if (wait && !m_Works.HasWorks())
		{
			ReleaseSurplusNumberBlocks(1);
			if (!DoNextTask(threadTime, false))
			{
				ReleaseSurplusNumberBlocks(4);
				::Sleep(0);
			}
		}
	}

	// Работа была прервана. Независимо от причины нужно остановить работу потока БД, чтобы мы могли
	// завершить свою работу корректно (возможно, сохранив в файл оставшиеся накопленные результаты)
	if (m_pDBThread && m_pDBThread->joinable())
	{
		m_StopDBThread = true;
		m_pDBThread->join();
	}

	// Если работа была прервана, то блоки чисел, находящиеся в pendingWorks, нужно
	// переместить в m_NumBlocks, чтобы они были корректно уничтожены в деструкторе
	for (NumberBlock* p : pendingWorks)
		m_NumBlocks.push_back(p);

	const uint32_t endTime = ::GetTickCount();
	// NB: к этому моменту поток БД уже завершился, поэтому нет необходимости
	// захватывать критическую секцию m_DBCS для манипуляций с текущим файлом БД
	const bool isEnoughData = GetDataSize(m_pActiveChunk) > Const::DATA_SAVE_SIZE / 4;
	// Сохраним накопленные данные, только если было проверено хотя бы 1 число и: либо накопилось более
	// четверти обычного объёма данных, либо с момента последнего сохранения прошло не менее 30 секунд
	if (m_Last > m_Data.GetLast() && (isEnoughData || endTime - m_LastSaveTick >= 30000))
		SaveResults();

	bool newLine = m_Events->HasEvents(true);
	m_Events->PublishAll();

	aux::Printf(m_IsCancelled ? "%sProcess was interrupted by user\n" : "%s", newLine ? "" : "\n");
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::OnRangeCompleted()
{
	// NB: здесь мы можем не захватывать m_DBCS для работы с БД, так
	// как в данный момент очереди потока БД гарантированно пусты

	m_Progress.progress = 0;
	m_Events->OnRangeCompleted(m_Last.GetLength());

	if (m_Last.GetLength() >= 3 && m_Last >= m_pActiveChunk->GetFirst())
	{
		SaveResults();
		CreateNewChunk(m_Last + 1u);
	}

	// В принципе мы можем продолжить поиск вплоть до длины чисел Const::MAX_DIGIT_C. Но сейчас длина чисел,
	// добавляемых в набор отсева, также ограничена этим значением (из-за ограничений FixNumber). И чтобы
	// отсев "работал", длина чисел в наборе должна быть хотя бы на 2 знака больше, чем текущий диапазон
	return m_Last.GetLength() < Const::MAX_DIGIT_C - 2;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::UpdateStepLimit(unsigned& stepLimit, const Number& number)
{
	const unsigned oldLimit = stepLimit;
	stepLimit = m_Steps->GetSearchLimit(number);

	if (oldLimit != stepLimit)
	{
		m_Events->OnSearchDepthChanged(stepLimit);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::PrintProgress(uint32_t tick, const Number& lastNum)
{
	m_Events->PublishAll();

	const uint32_t secElapsed = (tick - m_Progress.startTime) / 1000;
	m_Progress.totalSeconds += secElapsed;
	m_Progress.startTime += 1000 * secElapsed;

	const uint32_t elapsed = tick - m_Progress.lastTick;
	const float newSpeed = (elapsed > 50) ? 1000.f * m_Progress.counter / elapsed : m_Progress.lastSpeed;
	const float speed = (m_Progress.lastSpeed <= 0) ? newSpeed : .5f * (m_Progress.lastSpeed + newSpeed);

	m_Progress.counter = 0;
	m_Progress.lastTick = tick;
	m_Progress.lastSpeed = newSpeed;

	size_t numberC = 0, dataSize = 0;
	thread::Lock<thread::CriticalSection> lock(m_DBCS);
	if (const DBChunk* pChunk = m_pActiveChunk)
	{
		numberC = pChunk->GetNumbers().size();
		dataSize = GetDataSize(pChunk);
	}
	lock.Leave();

	aux::Printf("#8\r[%u] #15#%s#7 [%s], %u/%s, %.3f%% done...     \b\b\b\b\b",
		m_WorkThreads.GetThreadC() + 1, SeparateWithCommas(lastNum).c_str(), FormatSpeed(speed).c_str(), numberC,
		FormatSize(dataSize, true).c_str(), GetRangeProgress(lastNum.GetLength(), m_Progress.progress));

	if (util::SystemConsole::Instance().IsCtrlCPressed(false))
		aux::Printc("\b\b\b, #12stopping...");
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::UpdateProgress(const Number& lastNum)
{
	bool forcePrintProgress = false;
	if (m_PublishEvents.exchange(false, std::memory_order_acquire))
	{
		// Принудительно выведем прогресс, только если в очереди есть события, выводимые на экран
		forcePrintProgress = m_Events->HasEvents(true);
		m_Events->PublishAll();
	}

	const uint32_t tick = ::GetTickCount();
	if (tick - m_Progress.lastTick >= 500 || forcePrintProgress)
	{
		PrintProgress(tick, lastNum);

		auto& console = util::SystemConsole::Instance();
		if (console.IsCtrlCPressed(true))
			return false;

		int count = 0;
		util::Console::KeyEvent event;
		while (console.GetInputEvent(event))
		{
			// Клавиши - и + на малой клавиатуре
			if (event.isKeyDown && (event.key == VK_SUBTRACT || event.key == VK_ADD))
				count += (event.key == VK_ADD) ? 1 : -1;
			// Комбинации клавиш CTRL+[ (уменьшить) и CTRL+] (увеличить)
			else if (event.isKeyDown && event.isCtrlDown && (event.key == VK_OEM_4 || event.key == VK_OEM_6))
				count += (event.key == VK_OEM_6) ? 1 : -1;
		}
		if (count && !m_IsCancelled)
			m_WorkThreads.AddRemove(count);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::CreateNewChunk(const Number& first)
{
	m_pActiveChunk = new DBChunk;
	// NB: при текущем размере несжатого блока данных файла БД Const::DATA_SAVE_SIZE в 920 KiB,
	// файл в среднем содержит от 110К до 130К палиндромов, иногда больше. Так как мы сохраняем
	// в каждый файл лишь половину этих данных, то зарезервируем сразу 75K элементов (~1.43 MiB),
	// чтобы избежать потом лишних ресайзов контейнера
	m_pActiveChunk->Init(first, 75000);

	m_Data.SetActiveChunk(m_pActiveChunk);
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::SaveResults()
{
	Assert(m_pActiveChunk);

	const unsigned timeSpent = static_cast<unsigned>(m_CPUTime / 1000);
	const unsigned minSavedStep = m_Steps->GetMinSaveable(m_Last);
	m_Data.Save(m_Last, minSavedStep, timeSpent);
	m_LastSaveTick = ::GetTickCount();
	m_CPUTime = 0;

	const size_t numberC = m_pActiveChunk->GetNumbers().size();
	const size_t dataSize = GetDataSize(m_pActiveChunk, m_pActiveChunk->GetDataSize());
	m_pActiveChunk->UnloadData(DBChunkState::DATAUNLOADED);

	// Добавим кастомное событие, предназначенное только для файла журнала
	m_Events->OnCustomEvent(util::Format("Results saved [%u/%.0fKiB]",
		numberC, (1.f / 1024) * dataSize), true);
	// Выставим флаг, чтобы это и другие накопленные события были выведены
	// главным потоком сразу после обработки следующего готового задания
	m_PublishEvents.store(true, std::memory_order_release);
}

//----------------------------------------------------------------------------------------------------------------------
SearchMode::NumberBlock* SearchMode::GetNumberBlock()
{
	if (m_NumBlocks.empty())
		return new NumberBlock;

	NumberBlock* pBlock = m_NumBlocks.back();
	m_NumBlocks.pop_back();
	return pBlock;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::ReleaseSurplusNumberBlocks(size_t count)
{
	const size_t threadC = std::max(m_WorkThreads.GetThreadC(), size_t(1));
	const size_t minCount = 96 + 16 * threadC;

	if (m_NumBlocks.size() > minCount)
	{
		const size_t toDeleteC = std::min(count, m_NumBlocks.size() - minCount);
		for (size_t i = 0; i < toDeleteC; ++i)
		{
			NumberBlock* pBlock = m_NumBlocks.back();
			AML_SAFE_DELETE(pBlock);
			m_NumBlocks.pop_back();
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::ProcessWork(NumberBlock* pWork, unsigned stepLimit)
{
	Assert(pWork && stepLimit >= 100);

	m_SiftSetReaderC.fetch_and(~(1 << 31), std::memory_order_seq_cst);
	for (size_t spinC = 0; m_SiftSetReaderC.load(std::memory_order_acquire);)
	{
		_mm_pause();
		if (++spinC == 1000)
		{
			// В нормальном режиме (когда CPU не загружен на 100%, а потоки не вынуждены конкурировать)
			// количество циклов обычно равно 10-25 (при неэффективном отсеве) и 400-450 (при максимально
			// эффективном отсеве в конце диапазона). Поэтому после 1000 циклов отдаём управление системе
			// и ждём сигнала (который появится, когда счётчик достигнет 0)
			std::unique_lock<std::mutex> lock(m_SiftSetMutex);
			m_SiftSetCV.wait(lock);
			break;
		}
	}

	for (size_t i = 0; i < NumberBlock::SIZE; ++i)
	{
		const NumberItem& item = pWork->numA[i];
		if (item.stepDoneC >= stepLimit && !item.IsPalindrome())
			m_SiftSet.Insert(item.sifting);
	}

	// Устанавливаем 31-бит, разрешая чтение. Здесь нет гарантии, что значение счётчика равно 0, так
	// как рабочий поток мог, прочитав 1 в старшем бите, быть прерван. Он увеличит счётчик на 1, когда
	// будет возобновлён (например непосредственно перед нашей попыткой установить 31-й бит)
	m_SiftSetReaderC.fetch_or(1u << 31, std::memory_order_release);
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::ProcessDBWork(NumberBlock* pWork)
{
	Assert(pWork);

	size_t counter = 0;
	bool hasErrors = false;
	for (size_t i = 0; i < NumberBlock::SIZE; ++i)
	{
		const NumberItem& item = pWork->numA[i];

		if (item.IsValid())
		{
			++counter;
			if (item.IsPalindrome() && item.GetStepDoneC() > Const::MAX_STEP)
			{
				hasErrors = true;
				break;
			}
		}
	}
	m_Progress.counter += counter;
	m_Progress.progress += counter;
	return !hasErrors;
}

//----------------------------------------------------------------------------------------------------------------------
bool SearchMode::DoNextTask(ThreadTime& threadTime, bool waitIfNoTask)
{
	if (NumberBlock* pBlock = m_Tasks.PopTask(waitIfNoTask))
	{
		BigNumber num;
		for (size_t i = 0; i < NumberBlock::SIZE; ++i)
		{
			NumberItem& item = pBlock->numA[i];
			// NB: в неполных блоках (случается в конце диапазона), чисел будет меньше.
			// Для "отсутствующих" элементов поля siftLength и stepLimit будут равны 0
			if (!item.siftLength)
				break;

			num = item.num;
			unsigned stepDoneC = 0;
			if (num.RAATillLength(item.siftLength, stepDoneC))
				stepDoneC |= 0x80000000;
			else
			{
				item.sifting = num;
				if (stepDoneC < item.stepLimit)
				{
					bool isSifted = false;
					const uint32_t v = m_SiftSetReaderC.load(std::memory_order_relaxed) >> 31;
					if (v && (m_SiftSetReaderC.fetch_add(v, std::memory_order_acquire) & (1 << 31)))
						isSifted = m_SiftSet.Exists(item.sifting);
					if (!m_SiftSetReaderC.fetch_sub(v, std::memory_order_release))
						m_SiftSetCV.notify_one();

					if (!isSifted)
					{
						item.stepDoneC += stepDoneC;
						unsigned maxStepC = item.stepLimit - stepDoneC;
						if (num.RAATillPalindrome(maxStepC, stepDoneC))
							stepDoneC |= 0x80000000;
					}
				}
			}
			item.stepDoneC += stepDoneC;
		}

		pBlock->cpuTime += threadTime.GetElapsed(true);
		m_Works.PushWork(pBlock);
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void SearchMode::DBThreadFN()
{
	// Поток базы данных должен иметь повышенный приоритет
	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	Number num;
	ThreadTime threadTime;
	bool allowSave = true;

	while (!m_StopDBThread && !m_IsCancelled)
	{
		unsigned stepLimit = 0;
		if (NumberBlock* pWork = m_DBQueue.PopTask(stepLimit))
		{
			Assert(stepLimit);

			if (allowSave)
			{
				m_CPUTime += pWork->cpuTime;
				m_Last = pWork->lastNum;

				for (size_t i = 0; i < NumberBlock::SIZE; ++i)
				{
					const NumberItem& item = pWork->numA[i];

					if (!item.IsValid())
						continue;

					num = item.num;
					if (!item.IsPalindrome())
						m_Data.AddLychrel(num, stepLimit);
					else
					{
						const unsigned totalStepDoneC = item.GetStepDoneC();
						if (totalStepDoneC > Const::MAX_STEP)
						{
							m_Events->OnCustomEvent(util::Format("#12FATAL ERROR: a number"
								" is found for HUGE step #10#%u#12!", totalStepDoneC));
							allowSave = false;
							m_Last = --num;
							break;
						}
						else if (m_Steps->IsSaveable(num.GetLength(), totalStepDoneC))
						{
							bool alreadyFound = m_Data.HasFound(totalStepDoneC);
							m_Data.AddPalindrome(num, totalStepDoneC);
							if (!alreadyFound && m_Steps->IsNew(totalStepDoneC))
								m_Events->OnPalindromeFound(num, totalStepDoneC);
						} else
							m_Data.AddPalindrome(totalStepDoneC);
					}
				}
				m_CPUTime += threadTime.GetElapsed(true);

				if (allowSave && !m_IsCancelled)
				{
					thread::Lock<thread::CriticalSection> lock(m_DBCS);
					// Формируем файлы, содержащие половину нормального объёма данных. Из-за того, что в
					// старших диапазонах сохраняется меньше палиндромов, а скорость проверки чисел ниже,
					// часто будет возникать ситуация, когда за 15 минут не будет набираться нужный объём.
					// Позже во время операции обновления БД мы будем попарно объединять такие файлы
					const bool isEnoughData = GetDataSize(m_pActiveChunk) >= Const::DATA_SAVE_SIZE / 2 ||
						m_pActiveChunk->GetNumbers().size() >= Const::DATA_SAVE_NUMC / 2;
					if (isEnoughData || ::GetTickCount() - m_LastSaveTick >= Const::DATA_SAVE_TIME)
					{
						SaveResults();
						CreateNewChunk(m_Last + 1u);
					}
				}
			}

			m_DBQueue.PushWork(pWork);
		}
	}
}
