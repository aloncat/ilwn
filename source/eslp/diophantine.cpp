//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include "powers.h"
#include "threadtimer.h"
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
static bool OpenLogFile(util::File& file, int power, int count)
{
	if (file.Open(util::Format(L"log-%i.1.%i.txt", power, count),
		util::FILE_OPEN_ALWAYS | util::FILE_OPEN_READWRITE))
	{
		if (auto fileSize = file.GetSize(); fileSize > 0)
		{
			if (file.SetPosition(fileSize) && file.Write("---\n", 4))
				return true;
		}
		else if (fileSize == 0)
			return true;

		file.Close();
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void UpdateConsoleTitle(int power, int count, size_t solutionsFound = 0, size_t activeThreads = 0)
{
	auto s = util::Format("Searching for factors (%i.1.%i): %u solutions found", power, count, solutionsFound);
	s += activeThreads ? util::Format(" (%zu active threads)", activeThreads) : "";
	util::SystemConsole::Instance().SetTitle(s);
}

//--------------------------------------------------------------------------------------------------------------------------------
static void UpdateProgress(int factorCount, const unsigned* factors)
{
	int desiredCount = std::min((factorCount < 4) ? 1 : 2 + factorCount / 8, 8);
	auto s = util::Format("\rTesting %u=%u", factors[0], factors[1]);
	for (int i = 1; i < desiredCount; ++i)
	{
		s += '+';
		s += std::to_string(factors[i + 1]);
	}

	s += "+...";
	static size_t lastLength;
	auto newLength = s.size();
	if (newLength < lastLength)
	{
		auto k = lastLength - newLength;
		s.append(k, ' ').append(k, '\b');
	}
	lastLength = newLength;

	aux::Print(s);
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
	volatile bool isFinished = false;	// true, если поток завершился (вышел из функции)
	volatile bool shouldPause = false;	// true, если поток должен приостановить работу
	volatile bool shouldQuit = false;	// true, если поток должен завершиться

	ThreadTimer timer;					// Таймер затраченного потоком времени CPU

	unsigned factors[8];				// Заданные коэффициенты для начала поиска
	//unsigned factorCount = 0;			// Количество заданных коэффициентов

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

	UpdateConsoleTitle(power, count);
	aux::Printf("Searching for factors of equation #6#%i.1.%i#7, #2[Z]#7 starts from #10#%u\n",
		power, count, hiFactor);

	const unsigned maxFactor = Powers<UInt128>::CalcUpperValue(power, count);
	aux::Printf("Factor limit is set to #10#%s #8(128 bits)\n", SeparateWithCommas(maxFactor).c_str());

	if (hiFactor > maxFactor)
	{
		aux::Printc("Noting to do, exitting...");
		return false;
	}

	if (!OpenLogFile(m_Log, power, count))
	{
		aux::Printf("#12Error: failed to open log file\n");
		return false;
	}

	if (power > 2 && count == 2)
	{
		// Для уравнений вида p.1.2 при p > 2 просто предупреждаем
		aux::Printc("#12WARNING: #3This equation has no solutions!\n");
	}

	Search(hiFactor, maxFactor);
	m_Log.Close();

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::Search(unsigned hiFactor, unsigned maxFactor)
{
	aux::Print("Initializing...");
	m_MaxWorkerCount = util::SystemInfo::Instance().GetCoreCount().logical;
	m_ActiveWorkers = (m_MaxWorkerCount <= 4) ? m_MaxWorkerCount : m_MaxWorkerCount - 1;

	CreateWorkers(m_ActiveWorkers);
	
	// Работаем, пока не достигнут лимит значения старшего коэффициента и работа не прервана
	while (hiFactor && hiFactor <= maxFactor && !util::SystemConsole::Instance().IsCtrlCPressed())
	{
		hiFactor = Compute(hiFactor);
	}

	/*aux::Printf("\rTask %s#7, solutions found: #6#%u\n", isCancelled ?
		"#12cancelled" : "finished", m_Solutions.Count());

	if (hiFactor <= maxFactor)
	{
		auto s = util::Format("hiFactor: %u\n", hiFactor);
		m_Log.Write(s.c_str(), s.size());

		if (isCancelled)
		{
			aux::Printf("Factor (current value): #6#%u\n", hiFactor);
		}
	}

	const float timeElapsed = 0;//timer.Elapsed();
	aux::Printf((timeElapsed < 90) ? "Running time: %.2f#8s\n" : "Running time: %.0f#8s\n", timeElapsed);*/

	KillWorkers();
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
void FactorSearch::SetActiveThreads(size_t count)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned FactorSearch::Compute(unsigned hiFactor)
{
	aux::Print("\rInitializing...");

//-->
	// TODO: в зависимости от степени уравнения и количества коэффициентов, мы
	// бы хотели проверять различное количество старших коэффициентов за раз
	unsigned toCheck = 50000;
//<--

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
	GetPowersArray<NumberT>() = powers.GetData();

	m_Hashes.Init(upperLimit, powers);

	m_NextFactor = hiFactor;
	m_LastFactor = upperLimit;
	memset(m_Progress, 0, sizeof(m_Progress));

	// Пробуждаем все потоки
	SetActiveThreads(m_ActiveWorkers);

	uint32_t lastTick = 0;
	// Когда задания "закончатся", значение m_NextFactor станет равно 0
	while (m_NextFactor || GetActiveThreads(true))
	{
		if (uint32_t t = ::GetTickCount(); t - lastTick >= 500)
		{
			lastTick = t;

			unsigned factors[8];
			GetProgress(factors);

			UpdateProgress(m_FactorCount, factors);
			UpdateConsoleTitle(m_Power, m_FactorCount, m_Solutions.Count(), m_ActiveWorkers);

			if (util::SystemConsole::Instance().IsCtrlCPressed())
				m_IsCancelled = true;
		}

		::Sleep(20);
	}

	m_Pow64 = nullptr;
	m_Powers = nullptr;

	return upperLimit + 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
const NumberT*& FactorSearch::GetPowersArray()
{
	if constexpr (std::is_same_v<NumberT, uint64_t>)
		return m_Pow64;
	else
		return m_Powers;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::WorkerMainFn(Worker* worker)
{
	worker->isActive = true;
	HANDLE threadHandle = ::GetCurrentThread();
	::SetThreadPriority(threadHandle, THREAD_PRIORITY_IDLE);

	while (!worker->shouldQuit)
	{
		if (worker->shouldPause)
		{
			worker->isActive = false;
			::SuspendThread(threadHandle);
			worker->isActive = true;
		}

		/*if (!GetNextTask(worker))
		{
			worker->shouldPause = true;
			continue;
		}

		if (m_Powers)
			SearchFactors(worker, m_Powers);
		else
			SearchFactors(worker, m_Pow64);

		OnTaskDone(worker);*/
		::Sleep(10);
	}

	worker->isFinished = true;
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
		if (!(++it & 0x3fff) && !(++worker->progressCounter & 0x3f))
			OnProgress(worker->workerId, k);

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
AML_NOINLINE void FactorSearch::OnProgress(int id, const unsigned* factors)
{
	if (id == m_LowestWorkerId)
	{
		thread::Lock lock(m_ProgressCS);

		for (int i = 0; i < 8; ++i)
			m_Progress[i] = factors[i];
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnSolutionFound(const unsigned* factors)
{
	// TODO: хотя бы синхронизацию, а лучше пихать в вектор с синхронизацией, а
	// главный поток пусть в цикле проверяет и разбирает, учитывая очередность

	// Формируем решение и пытаемся его добавить в набор. Если
	// решение непримитивное, то функция Insert вернёт false
	Solution sol(factors, 1, m_FactorCount);
	if (m_Solutions.Insert(sol))
	{
		// Формируем строку с коэффициентами
		auto s = std::to_string(factors[0]) + '=';

		for (int i = 0, k; i < m_FactorCount; i += k)
		{
			k = 1;
			const unsigned f = factors[i + 1];
			while (i + k < m_FactorCount && f == factors[i + k + 1])
				++k;

			if (i)
			{
				s += '+';
			}
			if (k > 1)
			{
				s += std::to_string(k);
				s += '*';
			}
			s += std::to_string(f);
		}

		s += "\n";
		// Выводим строку с решением на экран и в файл
		aux::Printf("\rSolution found: %s", s.c_str());
		m_Log.Write(s.c_str(), s.size());
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void FactorSearch::GetProgress(unsigned* factors)
{
	thread::Lock lock(m_ProgressCS);

	for (int i = 0; i < 8; ++i)
		factors[i] = m_Progress[i];
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FactorSearch::GetNextTask(Worker* worker)
{
	thread::Lock lock(m_TaskCS);

	if (m_NextFactor && m_NextFactor <= m_LastFactor)
	{
		auto k0 = m_NextFactor++;
		m_PendingFactors.push_back(k0);
		m_LowestWorkerId = worker->workerId;
		//m_LowestPendingTask = m_PendingTasks.front();
		return true;
	} else
	{
		m_NextFactor = 0;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FactorSearch::OnTaskDone(Worker* worker)
{
	thread::Lock lock(m_TaskCS);

	for (size_t i = 0, count = m_PendingFactors.size(); i < count; ++i)
	{
		if (m_PendingFactors[i] == worker->factors[0])
		{
			/*if (i == 0)
			{
				m_LowestDoneTask = k0;
			}*/

			m_PendingFactors.erase(m_PendingFactors.begin() + i);
			//m_LowestPendingFaTask = m_PendingTasks.empty() ? 0 : m_PendingTasks.front();
			break;
		}
	}
}
