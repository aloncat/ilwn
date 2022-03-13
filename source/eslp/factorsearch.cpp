//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "factorsearch.h"

#include "options.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/debug.h>
#include <core/strformat.h>
#include <core/sysinfo.h>
#include <core/util.h>
#include <core/winapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::FactorSearch()
	: m_TaskCS(3000)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::~FactorSearch()
{
	KillWorkers();
	AML_SAFE_DELETE(m_NextTask);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::Search(const Options& options, const std::vector<unsigned>& startFactors)
{
	if (!Verify(!startFactors.empty()))
		return;

	m_MainThreadTimer.Reset();

	m_Solutions.Clear();
	m_SolutionsFound = 0;
	m_PendingSolutions.clear();
	m_PendingSolutions.reserve(100);
	m_PendingTasks.reserve(64);
	m_LastDoneHiFactor = 0;

	m_NoTasks = false;
	m_IsCancelled = false;
	m_IsAborted = false;
	m_ForceQuit = false;

	m_LastProgressLength = 15;
	aux::Print("Initializing...");
	util::SystemConsole::Instance().ShowCursor(false);

	size_t maxWorkerCount = util::SystemInfo::Instance().GetCoreCount().logical;
	m_ActiveWorkers = maxWorkerCount - ((maxWorkerCount <= 4) ? 0 : 1);

	if (options.HasOption("thread"))
	{
		int count = options["thread"].GetNumericValue();
		m_ActiveWorkers = util::Clamp(count, 1, static_cast<int>(m_ActiveWorkers));
	}

	if (m_Info.power == 1)
	{
		// NB: для степени 1 принудительно ограничиваем количество рабочих
		// потоков, так как для этой степени решения ищутся очень быстро
		m_ActiveWorkers = 1;
	}

	m_PrintSolutions = true;
	m_PrintAllSolutions = options.HasOption("printall");

	m_NextTask = new Task;
	CreateWorkers(maxWorkerCount);

	for (auto factors = startFactors; factors[0] && !m_IsCancelled;)
	{
		factors[0] = Compute(factors);
		factors.resize(1);

		m_Solutions.Clear();
	}

	UpdateConsoleTitle();

	KillWorkers();
	AML_SAFE_DELETE(m_NextTask);

	util::SystemConsole::Instance().ShowCursor(true);
	aux::Printf(m_IsAborted ? "\rTask #12ABORTED#7: too many solutions\n" :
		"\rTask %s#7, solutions found: #6#%u\n", m_IsCancelled ?
		"#12cancelled" : "finished", m_SolutionsFound);

	// NB: если было найдено максимально допустимое количество решений, то не все
	// решения попали в вывод, и m_LastDoneHiFactor не соответствует последнему
	// выведенному решению. Поэтому в этом случае значение выводить не стоит
	if (m_SolutionsFound < MAX_SOLUTIONS && m_LastDoneHiFactor < UINT_MAX)
	{
		auto s = util::Format("nextHiFactor: %u\n", m_LastDoneHiFactor + 1);
		m_Log.Write(s.c_str(), s.size());
	}

	const float timeElapsed = GetRunningTime();
	aux::Printf((timeElapsed < 90) ? "Running time: %.2f#8s\n" : "Running time: %.0f#8s\n", timeElapsed);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::UpdateConsoleTitle()
{
	util::Formatter<char> fmt;
	fmt << "Searching for factors (" << m_Info.power << '.' << m_Info.leftCount << '.' << m_Info.rightCount << "): ";
	fmt << m_SolutionsFound << ((m_SolutionsFound == 1) ? " solution" : " solutions") << " found";

	const size_t activeThreads = GetActiveThreads(true);
	if (activeThreads || (m_ActiveWorkers && !m_IsCancelled))
	{
		fmt << " -- MT: ";
		if (!m_IsCancelled && activeThreads != m_ActiveWorkers)
			fmt << m_ActiveWorkers << " (" << activeThreads << ')';
		else
			fmt << activeThreads;

		if (m_IsCancelled)
		{
			fmt << " -- stopping...";
		}

		// Если текущее количество активных потоков не совпадает с необходимым, то
		// установим флаг обновления заголовка окна, чтобы снова обновить его позже
		if (activeThreads != m_ActiveWorkers)
			m_NeedUpdateTitle = true;
	}

	util::SystemConsole::Instance().SetTitle(fmt.ToString());
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::InitHashTable(PowersBase& powers, unsigned upperLimit)
{
	if (m_Pow64)
		m_Hashes.Init(upperLimit, static_cast<Powers<uint64_t>&>(powers));
	else
		m_Hashes.Init(upperLimit, static_cast<Powers<UInt128>&>(powers));
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned FactorSearch::GetChunkSize(unsigned hiFactor)
{
	const int factorCount = m_Info.leftCount + m_Info.rightCount;
	// В зависимости от количества коэффициентов в уравнении, мы бы
	// хотели проверять различное количество старших коэффициентов за 1 раз
	static const unsigned countImpact[10] = { 0, 0, 0, 80000, 8000, 2200, 800, 640, 420, 240 };
	unsigned toCheck = (factorCount >= 3 && factorCount <= 9) ? countImpact[factorCount] : 120;
	// Для чётных степеней, не превышающих 8, увеличиваем значение в 1.5 раза
	toCheck += ((m_Info.power & 1) || m_Info.power > 8) ? 0 : toCheck / 2;

	return toCheck;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());

	task.factorCount = m_Info.leftCount;
	const int count = std::min(task.factorCount, static_cast<int>(startFactors.size()));

	for (size_t i = 0; i < count; ++i)
		task.factors[i] = startFactors[i];

	for (int i = count; i < task.factorCount; ++i)
		task.factors[i] = 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::SelectNextTask(Task& task)
{
	unsigned* k = task.factors;
	for (int i = task.factorCount - 1; i; --i)
	{
		if (k[i - 1] > k[i])
		{
			++k[i];
			return;
		}

		k[i] = 1;
	}

	++k[0];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
const NumberT*& FactorSearch::PowersArray()
{
	if constexpr (std::is_same_v<NumberT, uint64_t>)
		return m_Pow64;
	else
		return m_Powers;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FactorSearch::OnProgress(const Worker* worker, const unsigned* factors)
{
	m_ProgressMan.SetProgress(factors, worker->task.taskId);
	return m_ForceQuit;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FactorSearch::OnSolutionFound(const Worker* worker, const unsigned* factors)
{
	Solution solution(factors, m_Info.leftCount, m_Info.rightCount);
	solution.SortFactors();

	// Будем пропускать решения, в которых количество коэффициентов слева и справа одинаково и
	// при этом старший коэффициент правой части больше или равен старшего коэффициента в левой
	if (m_Info.leftCount == m_Info.rightCount && solution.left[0] <= solution.right[0])
		return m_IsAborted;

	// Если решение вырождено, то также пропустим его
	if (solution.IsConfluent())
		return m_IsAborted;

	for (;;)
	{
		m_TaskCS.Enter();

		// Если это решение самого младшего (старого) задания, то мы всегда добавляем его в контейнер.
		// В ином случае проверяем, сколько решений в контейнере, и если он "переполнен", то остальные
		// потоки (решения других заданий) должны подождать, когда освободится место
		if (auto count = m_PendingSolutions.size(); count < 5000 || worker->workerId <= m_LoPendingTask)
		{
			if (count >= 1000000)
			{
				m_IsCancelled = true;
				m_IsAborted = true;
				m_ForceQuit = true;
			}
			// Отсеиваем непримитивные решения. NB: так как проверка делается функцией класса Solutions,
			// который не потокобезопасен, то проверяем примитивность решения внутри критической секции
			else if (m_Solutions.IsPrimitive(solution))
			{
				m_PendingSolutions.push_back(std::move(solution));
			}

			m_TaskCS.Leave();
			return m_IsAborted;
		}

		// Освобождаем критическую секцию, и ждём. Если поток, обрабатывающий младшее
		// задание за это время его закончит, то место в контейнере освободится
		m_TaskCS.Leave();
		::Sleep(1);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::CreateWorkers(size_t threadCount)
{
	Assert(threadCount && m_Workers.empty());

	m_Workers.reserve(32);
	for (size_t id = 1; id <= threadCount; ++id)
	{
		auto worker = new Worker;
		m_Workers.push_back(worker);

		worker->workerId = static_cast<int>(id);

		worker->isActive = true;
		worker->shouldPause = true;

		worker->threadObj = std::thread([this, worker]() {
			worker->timer = new ThreadTimer;
			WorkerMainFn(worker);
		});
	}

	// NB: ждём пока все потоки войдут в главные функции и приостановятся. Т.к.
	// оба флага isActive и shouldPause были установлены, то потоки считаются
	// активными, не зависимо от того, начали они работу или ещё нет
	while (GetActiveThreads(true))
		::Sleep(1);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::KillWorkers()
{
	// Если по какой-то причине некоторые потоки сейчас активны, установим
	// флаги и подождём, пока они либо закончат работу либо приостановятся
	for (Worker* worker : m_Workers)
	{
		worker->shouldPause = true;
		worker->shouldQuit = true;
	}
	while (GetActiveThreads(true))
		::Sleep(1);

	// Завершаем оставшиеся потоки
	for (Worker* worker : m_Workers)
	{
		worker->shouldPause = false;
		if (worker->threadObj.joinable())
		{
			if (void* handle = worker->threadObj.native_handle())
				::ResumeThread(handle);

			worker->threadObj.join();
		}

		AML_SAFE_DELETE(worker->timer);
		delete worker;
	}

	m_Workers.clear();
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t FactorSearch::GetActiveThreads(bool ignorePending) const
{
	size_t activeCount = 0;
	for (Worker* worker : m_Workers)
	{
		if (worker->isActive && !worker->isFinished)
		{
			if (!worker->shouldPause || ignorePending)
				++activeCount;
		}
	}

	return activeCount;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::SetActiveThreads(size_t activeCount)
{
	activeCount = util::Clamp(activeCount, size_t(1), m_Workers.size());
	if (auto current = GetActiveThreads(); current != activeCount)
	{
		// Если доступных заданий нет, то мы не пробуждаем потоки
		if (current < activeCount && m_NoTasks)
			return;

		for (Worker* worker : m_Workers)
		{
			Assert(worker && !worker->isFinished);

			if (current < activeCount)
			{
				// Пробуждаем поток
				if (!worker->shouldQuit && (!worker->isActive || worker->shouldPause))
				{
					worker->shouldPause = false;
					if (!worker->isActive && worker->threadObj.joinable())
					{
						if (void* handle = worker->threadObj.native_handle())
							::ResumeThread(handle);
					}
					if (++current == activeCount)
						break;
				}
			}
			// Приостанавливаем поток
			else if (worker->isActive && !worker->shouldPause)
			{
				worker->shouldPause = true;
				if (--current == activeCount)
					break;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t FactorSearch::GetThreadTimes()
{
	uint64_t totalTime = 0;
	for (Worker* worker : m_Workers)
	{
		if (worker && worker->timer)
		{
			totalTime += worker->timer->GetElapsed(true);
		}
	}

	totalTime += m_MainThreadTimer.GetElapsed(true);
	return (totalTime + 500) / 1000;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::WorkerMainFn(Worker* worker)
{
	HANDLE threadHandle = ::GetCurrentThread();
	::SetThreadPriority(threadHandle, THREAD_PRIORITY_IDLE);

	Assert(ProgressManager::MAX_COEFS <= Task::MAX_COUNT);
	for (int i = 0; i < ProgressManager::MAX_COEFS; ++i)
		worker->task.factors[i] = 1;

	while (!worker->shouldQuit)
	{
		if (worker->shouldPause)
		{
			worker->isActive = false;
			::SuspendThread(threadHandle);

			worker->isActive = true;
			continue;
		}

		if (!GetNextTask(worker))
		{
			worker->shouldPause = true;
			continue;
		}

		PerformTask(worker);
		OnTaskDone(worker);
	}

	worker->isFinished = true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::GetNextTask(Worker* worker)
{
	if (!m_NoTasks && !m_IsCancelled)
	{
		thread::Lock lock(m_TaskCS);

		while (m_NextTask->factors[0] <= m_LastHiFactor)
		{
			if (MightHaveSolution(*m_NextTask))
			{
				worker->task = *m_NextTask;

				m_PendingTasks.push_back(worker->workerId);
				m_LoPendingTask = m_PendingTasks.front();

				SelectNextTask(*m_NextTask);
				++m_NextTask->taskId;

				return true;
			}

			// NB: выбираем следующее задание, но не меняем его
			// номер, так как задание не отправлено в обработку
			SelectNextTask(*m_NextTask);
		}
	}

	m_NoTasks = true;
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnTaskDone(Worker* worker)
{
	thread::Lock lock(m_TaskCS);

	for (size_t i = 0, count = m_PendingTasks.size(); i < count; ++i)
	{
		if (m_PendingTasks[i] == worker->workerId)
		{
			m_ProgressMan.SetDone(worker->task.taskId, !i);
			m_PendingTasks.erase(m_PendingTasks.begin() + i);

			if (!i)
			{
				m_LoPendingTask = (count > 1) ? m_PendingTasks.front() : 0;

				if (!m_ForceQuit)
				{
					if (!m_PendingTasks.empty())
					{
						// Список ожидаемых заданий не пуст, значит последнее полностью завершённое
						// задание было расположено непосредственно перед самым первым ожидаемым
						Worker* nextWorker = m_Workers[m_PendingTasks.front() - 1];
						m_LastDoneHiFactor = nextWorker->task.factors[0] - 1;
					}
					// Если же список пуст, то последнее полностью завершённое задание
					// было расположено непосредственно перед следующим (неназначенным)
					else if (unsigned nextHiFactor = m_NextTask->factors[0]; nextHiFactor > m_LastDoneHiFactor)
						m_LastDoneHiFactor = nextHiFactor - 1;
				}

				if (!m_PendingSolutions.empty())
					ProcessPendingSolutions();
			}

			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
std::pair<unsigned, unsigned> FactorSearch::CalcUpperValue(unsigned leftHigh) const
{
	// Находим предельное значение для коэффициента
	const auto maxFactorCount = std::max(m_Info.leftCount, m_Info.rightCount);
	const unsigned upperLimit = Powers<NumberT>::CalcUpperValue(m_Info.power, maxFactorCount);

	// Рассчитываем желаемые максимальные значения старших коэффициентов левой и правой частей.
	// NB: так как в алгоритме перебора мы иногда используем большие приращения, нам необходимо,
	// чтобы значения коэффициентов не вышли за пределы 32-бит. Для этого немного ограничим лимит
	leftHigh = std::min(leftHigh, std::min(upperLimit, UINT_MAX - 999));
	unsigned rightHigh = leftHigh;

	if (m_Info.leftCount > 1)
	{
		// NB: для уравнений, в которых в левой части 2 и более коэффициента, макс. значение старшего
		// коэффициента в правой части может быть выше значения старшего коэффициента в левой. Поэтому
		// нам нужно найти такое значение для старшего коэффициента правой части, при котором мин. возможная
		// сумма в правой части будет выше макс. возможной суммы в левой (при таких условиях мы не пропустим
		// возможных наборов коэффициентов правой части, которые могут дать решение)

		// Макс. возможные значения сумм степеней коэффициентов левой и правой частей
		auto leftPowHi = Powers<NumberT>::CalcPower(leftHigh, m_Info.power) * m_Info.leftCount;
		auto rightPowHi = Powers<NumberT>::CalcPower(rightHigh, m_Info.power) + (m_Info.rightCount - 1);

		// Найдём подходящее значение старшего коэффициента правой части
		while (leftPowHi >= rightPowHi && rightHigh < upperLimit)
		{
			++rightHigh;
			rightPowHi = Powers<NumberT>::CalcPower(rightHigh, m_Info.power) + (m_Info.rightCount - 1);
		}

		// Если значение rightHigh стало равно upperLimit, то поступим наоборот: уменьшим значение
		// старшего коэффициента левой части leftHigh, чтобы наше условие выполнялось для rightHigh
		while (leftPowHi >= rightPowHi)
		{
			--leftHigh;
			leftPowHi = Powers<NumberT>::CalcPower(leftHigh, m_Info.power) * m_Info.leftCount;
		}
	}

	return { leftHigh, rightHigh };
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned FactorSearch::Compute(const std::vector<unsigned>& startFactors)
{
	Assert(!startFactors.empty());
	aux::Print("\rRe-initializing...");

	// Определяем размер блока вычислений
	const unsigned toCheck = GetChunkSize(startFactors[0]);

	// Если можем, то выполняем вычисления в 64-битах (так как это быстрее)
	if (unsigned upper64 = CalcUpperValue<uint64_t>().first; startFactors[0] < upper64)
		return Compute<uint64_t>(startFactors, toCheck);

	return Compute<UInt128>(startFactors, toCheck);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
unsigned FactorSearch::Compute(const std::vector<unsigned>& startFactors, unsigned toCheck)
{
	Assert(!startFactors.empty() && toCheck && "Nothing to test");
	Assert(!GetActiveThreads(true) && "All threads must be suspended");

	const unsigned hiFactor = startFactors[0];
	const auto upperLimit = CalcUpperValue<NumberT>(hiFactor + std::min(UINT_MAX - hiFactor, toCheck - 1));

	if (!hiFactor || hiFactor > upperLimit.first)
		return 0;

	Powers<NumberT> powers;
	powers.Init(m_Info.power, 1, upperLimit.second);
	PowersArray<NumberT>() = powers.GetData();

	InitHashTable(powers, upperLimit.second);

	InitFirstTask(*m_NextTask, startFactors);
	m_LastDoneHiFactor = hiFactor - 1;
	m_LastHiFactor = upperLimit.first;

	Assert(m_PendingTasks.empty());
	m_LoPendingTask = 0;
	m_NoTasks = false;

	m_ProgressMan.Reset();
	m_ForceShowProgress = true;
	m_NeedUpdateTitle = true;

	// Активируем рабочие потоки
	SetActiveThreads(m_ActiveWorkers);

	uint32_t lastProgressTick = 0;
	while (!m_NoTasks || GetActiveThreads(true))
	{
		if (m_ForceShowProgress || ::GetTickCount() - lastProgressTick >= 500)
		{
			unsigned factors[8];
			if (m_ProgressMan.GetProgress(factors))
			{
				ShowProgress(factors);
				m_ForceShowProgress = false;
				lastProgressTick = ::GetTickCount();
			}

			UpdateRunningTime();
		}

		if (m_NeedUpdateTitle || m_IsCancelled)
		{
			m_NeedUpdateTitle = false;
			UpdateConsoleTitle();
		}

		if (util::SystemConsole::Instance().IsCtrlCPressed(true))
		{
			if (m_IsCancelled)
				m_ForceQuit = true;
			else
			{
				m_IsCancelled = true;
				m_ForceShowProgress = true;
			}
		}

		UpdateActiveThreadCount();
		::Sleep(m_ForceShowProgress || m_NoTasks ? 1 : 20);
	}

	m_Pow64 = nullptr;
	m_Powers = nullptr;

	Assert(!GetActiveThreads(true) && "Some threads are still active");
	Assert(m_IsCancelled || m_NextTask->factors[0] >= upperLimit.first + 1);

	if (m_LastProgressLength)
	{
		// Очистим строку на экране, в которой выводился прогресс
		aux::Print("\r" + std::string(m_LastProgressLength, ' ') + "\r");
		m_LastProgressLength = 0;
	}

	return upperLimit.first + 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::ShowProgress(const unsigned* factors)
{
	const int factorCount = m_Info.leftCount + m_Info.rightCount;
	const int desiredCount = (factorCount < 5) ? 2 : std::min(8, 3 + (factorCount - 1) / 8);

	util::Formatter<char> fmt;
	fmt << (m_IsCancelled ? "#12" : "#07") << "\rTesting " << factors[0];
	for (int i = 1, j = std::min(m_Info.leftCount, desiredCount); i < j; ++i)
		fmt << '+' << factors[i];

	if (desiredCount >= m_Info.leftCount)
	{
		fmt << '=';
		for (int i = m_Info.leftCount; i < desiredCount; ++i)
			fmt << factors[i] << '+';
	} else
	{
		fmt << '+';
	}

	fmt << "...";
	auto newSize = fmt.GetSize() - 4;
	if (newSize < m_LastProgressLength)
	{
		auto k = m_LastProgressLength - newSize;
		for (size_t i = 0; i < k; ++i)
			fmt << ' ';
		for (size_t i = 0; i < k; ++i)
			fmt << '\b';
	}
	m_LastProgressLength = newSize;

	thread::Lock lock(m_ConsoleCS);
	aux::Printc(fmt);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnSolutionReady(const Solution& solution)
{
	util::Formatter<char> fmt;

	auto fn = [&](const std::vector<unsigned>& factors) {
		for (size_t i = 0, k, count = factors.size(); i < count; i += k)
		{
			k = 1;
			const unsigned f = factors[i];
			while (i + k < count && f == factors[i + k])
				++k;

			if (i)
			{
				fmt << '+';
			}
			if (k > 1)
			{
				fmt << k << '*';
			}
			fmt << f;
		}
	};

	fn(solution.left);
	fmt << '=';
	fn(solution.right);
	fmt << '\n';

	// Выводим решение в файл
	m_Log.Write(fmt.GetData(), fmt.GetSize());

	if (m_PrintSolutions)
	{
		thread::Lock lock(m_ConsoleCS);

		// Выводим решение на экран (первые 1000 решений - в любом
		// случае, следующие - только при наличии опции "--printall")
		if (m_SolutionsFound <= 1000 || m_PrintAllSolutions)
			aux::Print("\rSolution found: " + fmt.ToString());
		else
		{
			aux::Printc("#12\rToo many solutions found: #7no more will be shown\n");
			m_PrintSolutions = false;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::ProcessPendingSolutions()
{
	if (m_ForceQuit)
	{
		// NB: при форсированном выходе потоки не завершают задания, поэтому мы
		// не можем гарантировать, что в вывод попадут все промежуточные решения
		m_PendingSolutions.clear();
	}
	else if (size_t count = m_PendingSolutions.size())
	{
		size_t ready = count;
		if (m_LoPendingTask)
		{
			ready = 0;
			const Task& lowestTask = m_Workers[m_LoPendingTask - 1]->task;
			// NB: если имеются ожидаемые задания (результаты которых ещё не полностью готовы), то мы можем
			// обработать только часть решений, соответствующих полностью завершённым заданиям. Поэтому для
			// эффективности (решений может быть довольно много) предварительно переместим все подходящие
			// решения в начало контейнера, а затем отсортируем только эту его часть
			for (size_t i = 0; i < count; ++i)
			{
				if (auto& s = m_PendingSolutions[i]; lowestTask > s)
					s.Swap(m_PendingSolutions[ready++]);
			}

			if (!ready)
				return;
		}

		// Сортируем готовые решения
		auto itEnd = m_PendingSolutions.begin() + ready;
		std::sort(m_PendingSolutions.begin(), itEnd);

		// Обрабатываем
		bool hadGoodSolutions = false;
		const bool consoleOutputAllowed = m_PrintSolutions;
		for (auto it = m_PendingSolutions.begin(); it != itEnd; ++it)
		{
			// Пытаемся добавить решение в набор. Вырожденные и непримитивные
			// мы уже отбросили. Сейчас проверяем только уникальность решения
			if (m_Solutions.Insert(*it, false))
			{
				hadGoodSolutions = true;
				auto found = ++m_SolutionsFound;

				// Выводим решение
				OnSolutionReady(*it);

				if (found >= MAX_SOLUTIONS)
				{
					m_IsCancelled = true;
					m_ForceQuit = true;
					break;
				}
			}
		}

		// Удаляем все обработанные решения из контейнера
		m_PendingSolutions.erase(m_PendingSolutions.begin(), itEnd);

		// Если непримитивные решения были, то они были выведены на экран.
		// Нам нужно немедленно вывести прогресс и обновить заголовок окна
		if (hadGoodSolutions)
		{
			m_ForceShowProgress |= consoleOutputAllowed;
			m_NeedUpdateTitle = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::UpdateActiveThreadCount()
{
	const auto oldValue = m_ActiveWorkers;
	auto& console = util::SystemConsole::Instance();
	for (util::Console::KeyEvent event; console.GetInputEvent(event);)
	{
		// Клавиша - на малой клавиатуре
		if (event.isKeyDown && event.vkey == util::Console::KEY_PADSUB)
		{
			if (m_ActiveWorkers > 1)
				--m_ActiveWorkers;
		}
		// Клавиша + на малой клавиатуре
		else if (event.isKeyDown && event.vkey == util::Console::KEY_PADADD)
		{
			if (m_ActiveWorkers < m_Workers.size())
				++m_ActiveWorkers;
		}
	}

	// NB: если флаг m_IsCancelled установлен, то нет смысла менять количество активных потоков, т.к.
	// работающие потоки, завершив текущее задание, не получат нового и автоматически приостановятся
	if (!m_IsCancelled && m_ActiveWorkers != oldValue)
	{
		SetActiveThreads(m_ActiveWorkers);
		m_NeedUpdateTitle = true;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch::Task
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::Task& FactorSearch::Task::operator =(const Task& that) noexcept
{
	if (this != &that)
	{
		factorCount = that.factorCount;
		taskId = that.taskId;

		if (factorCount < 8)
		{
			for (int i = 0; i < factorCount; ++i)
				factors[i] = that.factors[i];
		} else
		{
			memcpy(factors, that.factors, 4 * factorCount);
		}
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::Task::operator >(const Solution& rhs) const noexcept
{
	if (const int leftCount = static_cast<int>(rhs.left.size()); factorCount <= leftCount)
	{
		for (int i = 0; i < factorCount; ++i)
		{
			if (auto f = rhs.left[i]; factors[i] != f)
				return factors[i] > f;
		}
	} else
	{
		for (int i = 0; i < leftCount; ++i)
		{
			if (auto f = rhs.left[i]; factors[i] != f)
				return factors[i] > f;
		}

		for (int i = leftCount; i < factorCount; ++i)
		{
			if (auto f = rhs.right[i - leftCount]; factors[i] != f)
				return factors[i] > f;
		}
	}

	return false;
}
