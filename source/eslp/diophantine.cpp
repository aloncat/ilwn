//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include "powers.h"
#include "util.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/debug.h>
#include <core/strformat.h>
#include <core/sysinfo.h>
#include <core/winapi.h>

#include <thread>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
static bool OpenLogFile(util::File& file, int power, int leftCount, int rightCount)
{
	if (file.Open(util::Format(L"log-%i.%i.%i.txt", power, leftCount, rightCount),
		util::FILE_OPEN_ALWAYS | util::FILE_OPEN_READWRITE))
	{
		if (auto fileSize = file.GetSize(); fileSize > 0)
		{
			if (file.SetPosition(fileSize) && file.Write("---\n", 4))
				return true;
		}
		else if (!fileSize)
			return true;

		file.Close();
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch::Worker - состояние рабочего потока
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct FactorSearch::Worker
{
	int workerId = 0;					// Уникальный ID рабочего потока (от 0)
	std::thread threadObj;				// Объект потока

	volatile bool isActive = false;		// true, если поток выполняет вычисления
	volatile bool isFinished = false;	// true, если поток завершился (вышел из своей функции)
	volatile bool shouldPause = false;	// true, если поток должен приостановиться (не брать задание)
	volatile bool shouldQuit = false;	// true, если поток должен завершиться (после задания)

	ThreadTimer* timer = nullptr;		// Таймер затраченного потоком времени CPU

	unsigned factors[8];				// Заданные коэффициенты для начала поиска
	unsigned factorCount = 0;			// Количество заданных коэффициентов

	unsigned progressCounter = 0;		// Вспомогательный счётчик прогреса
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FactorSearch
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::FactorSearch()
	: m_TaskCS(3000)
	, m_ProgressCS(1000)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::~FactorSearch()
{
	KillWorkers();
	m_Log.Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::Run(int power, int count, unsigned hiFactor)
{
	if (power < 1 || power > MAX_POWER || count < 2 || count > MAX_FACTOR_COUNT)
	{
		aux::Printc("#12Error: parameters are incorrect");
		return false;
	}

	m_Power = power;
	m_FactorCount = count;

	m_StartTick = ::GetTickCount();
	m_WorkTime = 0;

	UpdateConsoleTitle(1, m_FactorCount);
	aux::Printf("Searching for factors of equation #6#%i.1.%i#7, #2[Z]#7 starts from #10#%u\n",
		m_Power, m_FactorCount, hiFactor);

	const unsigned maxFactor = Powers<UInt128>::CalcUpperValue(m_Power, m_FactorCount);
	aux::Printf("Factor limit is set to #10#%s #8(128 bits)\n", SeparateWithCommas(maxFactor).c_str());

	if (hiFactor > maxFactor)
	{
		aux::Printc("Noting to do, exitting...");
		return false;
	}

	if (!OpenLogFile(m_Log, power, 1, count))
	{
		aux::Printc("#12Error: failed to open log file\n");
		return false;
	}

	if (power > 2 && count == 2)
	{
		// Для уравнений вида p.1.2 при p > 2 просто предупреждаем
		aux::Printc("#12WARNING: #3this equation has no solutions!\n");
	}

	Search(hiFactor);
	m_Log.Close();

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::Search(unsigned hiFactor)
{
	m_Solutions.Clear();
	m_SolutionsFound = 0;
	m_PendingSolutions.clear();

	m_MainThreadTimer.Reset();
	m_IsCancelled = false;
	m_ForceQuit = false;

	m_LastProgressLength = 16;
	aux::Print("Initializing...");
	util::SystemConsole::Instance().ShowCursor(false);

	m_MaxWorkerCount = util::SystemInfo::Instance().GetCoreCount().logical;
	m_ActiveWorkers = m_MaxWorkerCount - ((m_MaxWorkerCount <= 4) ? 0 : 1);

	CreateWorkers(m_ActiveWorkers);

	while (hiFactor && !m_IsCancelled)
		hiFactor = Compute(hiFactor);

	UpdateConsoleTitle(1, m_FactorCount);
	KillWorkers();

	util::SystemConsole::Instance().ShowCursor(true);
	aux::Printf("\rTask %s#7, solutions found: #6#%u\n", m_IsCancelled ?
		"#12cancelled" : "finished", m_Solutions.Count());

	auto s = util::Format("nextHiFactor: %u\n", m_LastDoneFactor + 1);
	m_Log.Write(s.c_str(), s.size());

	const float timeElapsed = m_WorkTime + .001f * (::GetTickCount() - m_StartTick);
	aux::Printf((timeElapsed < 90) ? "Running time: %.2f#8s\n" : "Running time: %.0f#8s\n", timeElapsed);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::CreateWorkers(size_t threadCount)
{
	Assert(threadCount && m_Workers.empty());

	m_Workers.reserve(64);
	for (size_t id = 0; id < threadCount; ++id)
	{
		auto worker = new Worker;
		m_Workers.push_back(worker);

		worker->workerId = static_cast<int>(id);
		worker->shouldPause = true;

		worker->threadObj = std::thread([this, worker]() {
			WorkerMainFn(worker);
		});
	}

	while (GetActiveThreads(true))
		::Sleep(1);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::KillWorkers()
{
	for (Worker* worker : m_Workers)
	{
		worker->shouldQuit = true;
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
	activeCount = util::Clamp(activeCount, size_t(1), m_MaxWorkerCount);
	if (auto current = GetActiveThreads(); current < activeCount)
	{
		// Пробуждаем потоки
		for (Worker* worker : m_Workers)
		{
			Assert(worker && !worker->isFinished);
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
	}
	else if (current > activeCount)
	{
		// Приостанавливаем потоки
		for (Worker* worker : m_Workers)
		{
			Assert(worker && !worker->isFinished);
			if (worker->isActive && !worker->shouldPause)
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
unsigned FactorSearch::Compute(unsigned hiFactor)
{
	aux::Print("\rRe-initializing...");

	// TODO: в зависимости от степени уравнения и количества коэффициентов, мы
	// бы хотели проверять различное количество старших коэффициентов за раз
	unsigned toCheck = 10000;

	// Если можем, то выполняем вычисления в 64-битах (так как это быстрее)
	if (unsigned upper64 = Powers<uint64_t>::CalcUpperValue(m_Power, m_FactorCount); hiFactor < upper64)
		return Compute<uint64_t>(hiFactor, toCheck);

	return Compute<UInt128>(hiFactor, toCheck);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
unsigned FactorSearch::Compute(unsigned hiFactor, unsigned toCheck)
{
	Assert(!GetActiveThreads(true) && "All threads must be suspended");

	const unsigned upperLimit = std::min(hiFactor + std::min(UINT_MAX - hiFactor,
		toCheck - 1), Powers<NumberT>::CalcUpperValue(m_Power, m_FactorCount));

	Powers<NumberT> powers;
	powers.Init(m_Power, m_FactorCount, upperLimit);
	PowersArray<NumberT>() = powers.GetData();
	m_Hashes.Init(upperLimit, powers);

	m_NextHiFactor = hiFactor;
	m_LastDoneFactor = hiFactor - 1;
	m_LastHiFactor = upperLimit;

	Assert(m_PendingTasks.empty());
	m_LoPendingTask = 0;
	m_NoTasks = false;

	memset(m_Progress, 0, sizeof(m_Progress));
	m_Progress[0] = hiFactor;

	m_IsProgressReady = false;
	m_ForceShowProgress = true;

	// Активируем рабочие потоки
	Assert(!GetActiveThreads(true));
	SetActiveThreads(m_ActiveWorkers);

	uint32_t lastProgressTick = 0;
	uint32_t lastTitleUpdateTick = 0;
	while (!m_NoTasks || GetActiveThreads(true))
	{
		const uint32_t tick = ::GetTickCount();
		if ((m_ForceShowProgress || tick - lastProgressTick >= 500) && m_IsProgressReady)
		{
			unsigned factors[8];
			GetProgress(factors);
			ShowProgress(factors, 1, m_FactorCount);

			m_ForceShowProgress = false;
			lastProgressTick = ::GetTickCount();

			const auto secElapsed = (lastProgressTick - m_StartTick) / 1000;
			m_StartTick += 1000 * secElapsed;
			m_WorkTime += secElapsed;
		}

		if (tick - lastTitleUpdateTick >= 250)
		{
			lastTitleUpdateTick = tick;
			UpdateConsoleTitle(1, m_FactorCount);
		}

		if (bool userBreak = util::SystemConsole::Instance().IsCtrlCPressed(true);
			userBreak || m_SolutionsFound >= 100000)
		{
			m_ForceShowProgress |= !m_IsCancelled;
			m_ForceQuit = m_IsCancelled && userBreak;
			m_IsCancelled = true;
		}

		::Sleep(m_ForceShowProgress ? 1 : 20);
	}

	m_Pow64 = nullptr;
	m_Powers = nullptr;

	Assert(!GetActiveThreads(true));
	Assert(m_IsCancelled || m_NextHiFactor == upperLimit + 1);

	if (m_LastProgressLength)
	{
		std::string s("\r");
		s.append(m_LastProgressLength, ' ');
		s.append(m_LastProgressLength, '\b');

		aux::Print(s);
		m_LastProgressLength = 0;
	}

	return upperLimit + 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::WorkerMainFn(Worker* worker)
{
	worker->isActive = true;
	worker->timer = new ThreadTimer;

	HANDLE threadHandle = ::GetCurrentThread();
	::SetThreadPriority(threadHandle, THREAD_PRIORITY_IDLE);

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

		if (m_Pow64)
			SearchFactors(worker, m_Pow64);
		else
			SearchFactors(worker, m_Powers);

		OnTaskDone(worker);
	}

	worker->isFinished = true;
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
AML_NOINLINE void FactorSearch::GetProgress(unsigned* factors)
{
	thread::Lock lock(m_ProgressCS);

	for (int i = 0; i < 8; ++i)
		factors[i] = m_Progress[i];
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void FactorSearch::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Массив k хранит коэффициенты правой части уравнения, начиная с индекса 1.
	// В элементе с индексом 0 будем хранить коэффициент левой части уравнения
	unsigned k[1 + MAX_FACTOR_COUNT] = { worker->factors[0] };
	for (int i = 1; i <= m_FactorCount; ++i)
		k[i] = 1;

	// Значение левой части
	const auto z = powers[k[0]];
	// Пропускаем значения 1-го коэффициента в правой
	// части, при которых набор не может дать решение
	for (k[1] = 1; z > powers[k[1]] * m_FactorCount; ++k[1]);
	// Сумма всех членов правой части, кроме последнего
	auto sum = powers[k[1]] + m_FactorCount - 2;

	const int count = m_FactorCount;
	// Перебираем коэффициенты правой части
	for (size_t it = 0; k[0] > k[1];)
	{
		// Вычисляем значение степени последнего коэффициента правой части, при котором может существовать
		// решение уравнения, и проверяем значение в хеш-таблице. Если значение не будет найдено, то значит
		// не существует такого целого числа, степень которого равна lastFP и можно пропустить этот набор
		if (const auto lastFP = z - sum; m_Hashes.Exists(lastFP))
		{
			// Хеш был обнаружен. Теперь нужно найти число, соответствующее значению степени lastFP. Так
			// как массив степеней powers упорядочен по возрастанию, то используем бинарный поиск. Если
			// коллизии хешей не было, то мы обнаружим значение в массиве, а его индекс будет искомым
			// числом. Искомое значение не может превышать значения предпоследнего коэффициента
			for (unsigned lo = 1, hi = k[count - 1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[count] = mid;
					OnSolutionFound(k);
					break;
				}
			}
		}

		// Периодически выводим текущий прогресс. Если пользователь
		// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
		if (!(++it & 0x1fff) && !(++worker->progressCounter & 0x3f))
		{
			if (OnProgress(worker, k))
				break;
		}

		int idx = 0;
		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
		for (int i = count - 1;; --i)
		{
			sum -= powers[k[i]];
			if (k[i - 1] > k[i])
			{
				const auto f = ++k[i];
				if (auto n = sum + powers[f]; n < z || i == 1)
				{
					sum = n;
					break;
				}
			}
			sum += k[i] = 1;
			idx = i;
		}

		if (idx)
		{
			// Каждый раз, когда мы сбрасываем в 1 коэффициент в правой части, увеличивая на 1 коэффициент слева от него,
			// переменная idx будет содержать индекс самого левого единичного коэффициента. Единичное и многие следующие
			// значения коэффициента не смогут дать решений, так как для них сумма в правой части будет меньше значения
			// левой. Поэтому мы будем пропускать такие значения, сразу переходя к тем, которые могут дать решение
			unsigned hi = k[idx - 1];
			for (int rem = count - idx + 1; rem > 1; --rem)
			{
				const auto s = sum - (rem - 1);
				for (unsigned step = hi >> 1; step; step >>= 1)
				{
					auto f = k[idx] + step;
					if (hi >= f && z > s + powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx++];
				sum += powers[hi] - 1;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::GetNextTask(Worker* worker)
{
	thread::Lock lock(m_TaskCS);

	if (!m_NoTasks && !m_IsCancelled && m_NextHiFactor && m_NextHiFactor <= m_LastHiFactor)
	{
		auto task = m_NextHiFactor++;
		m_PendingTasks.push_back(task);
		m_LoPendingTask = m_PendingTasks.front();

		worker->factors[0] = task;
		worker->factorCount = 1;
		return true;
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
		if (auto f = worker->factors[0]; m_PendingTasks[i] == f)
		{
			m_PendingTasks.erase(m_PendingTasks.begin() + i);
			m_LoPendingTask = m_PendingTasks.empty() ? 0 : m_PendingTasks.front();

			if (!i && !m_PendingSolutions.empty())
				ProcessPendingSolutions();

			if (!i && !m_ForceQuit)
			{
				// Если список ожидаемых заданий не пуст, то последнее полностью завершённое - это то,
				// которое было непосредственно перед первым ожидаемым, иначе - только что завершённое
				m_LastDoneFactor = m_PendingTasks.empty() ?
					f : m_PendingTasks.front() - 1;
			}

			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FactorSearch::OnProgress(Worker* worker, const unsigned* factors)
{
	if (worker->factors[0] == m_LoPendingTask)
	{
		thread::Lock lock(m_ProgressCS);

		for (int i = 0; i < 8; ++i)
			m_Progress[i] = factors[i];

		m_IsProgressReady = true;
	}

	return m_ForceQuit;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void FactorSearch::OnSolutionFound(const unsigned* factors)
{
	Solution s(factors, 1, m_FactorCount);
	s.SortFactors();

	{
		thread::Lock lock(m_TaskCS);
		m_PendingSolutions.push_back(std::move(s));
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnSolutionReady(const Solution& solution)
{
	util::Formatter<char> fmt;

	auto fn = [&fmt](const std::vector<unsigned>& factors) {
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

	m_Log.Write(fmt.GetData(), fmt.GetSize());

	thread::Lock lock(m_ConsoleCS);
	aux::Print("\rSolution found: " + fmt.ToString());
	m_ForceShowProgress = true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::ProcessPendingSolutions()
{
	if (m_ForceQuit)
	{
		// NB: при форсированном выходе потоки не завершают задания, поэтому мы
		// не можем гарантировать, что в вывод попадут все промежуточные решения
		m_PendingSolutions.clear();
		return;
	}

	// Сортируем накопленные решения
	std::sort(m_PendingSolutions.begin(), m_PendingSolutions.end());

	// Обработаем готовые решения (до m_LoPendingTask)
	while (!m_PendingSolutions.empty())
	{
		auto& s = m_PendingSolutions.front();
		if (m_LoPendingTask && s.left[0] >= m_LoPendingTask)
			break;

		// Формируем решение и пытаемся его добавить в набор. Если
		// решение непримитивное, то функция Insert вернёт false
		if (m_Solutions.Insert(s))
		{
			++m_SolutionsFound;
			OnSolutionReady(s);
		}

		m_PendingSolutions.pop_front();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::ShowProgress(const unsigned* factors, int leftCount, int rightCount)
{
	const int factorCount = leftCount + rightCount;
	const int desiredCount = (factorCount < 5) ? 2 : std::min(8, 3 + (factorCount - 1) / 8);

	util::Formatter<char> fmt;
	fmt << (m_IsCancelled ? "#12" : "") << "\rTesting " << factors[0];
	for (int i = 1, j = std::min(leftCount, desiredCount); i < j; ++i)
		fmt << '+' << factors[i];

	if (desiredCount >= leftCount)
		fmt << '=';

	for (int i = leftCount; i < desiredCount; ++i)
		fmt << factors[i] << '+';

	fmt << "...";
	auto newSize = fmt.GetSize() - 1;
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
void FactorSearch::UpdateConsoleTitle(int leftCount, int rightCount)
{
	util::Formatter<char> fmt;
	fmt << "Searching for factors (" << m_Power << '.' << leftCount << '.' << rightCount << "): ";
	fmt << m_SolutionsFound << ((m_SolutionsFound == 1) ? " solution" : " solutions") << " found";

	if (auto activeCount = m_IsCancelled ? GetActiveThreads() : m_ActiveWorkers)
	{
		fmt << " (" << activeCount << " active threads)";

		if (m_IsCancelled)
		{
			fmt << " -- STOPPING...";
		}
	}

	util::SystemConsole::Instance().SetTitle(fmt.ToString());
}
