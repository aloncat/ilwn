//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "progressman.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ProgressManager
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
ProgressManager::ProgressManager()
	: m_CS(1000)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
void ProgressManager::Reset()
{
	m_Count = 0;
	m_IsReady = false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool ProgressManager::GetProgress(unsigned* factors) const
{
	thread::Lock lock(m_CS);

	const unsigned* progress;
	// Если у самого старого незавершённого задания (голова списка) есть данные,
	// то вернём именно их. Иначе вернём данные последнего завершённого задания
	if (m_Count && m_Items[m_Head].isReady)
		progress = m_Items[m_Head].progress;
	else if (m_IsReady)
		progress = m_Progress;
	else
		return false;

	for (size_t i = 0; i < MAX_COEFS; ++i)
		factors[i] = progress[i];

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void ProgressManager::SetProgress(const unsigned* factors, unsigned taskId)
{
	thread::Lock lock(m_CS);

	// Сохраняем прогресс задания
	if (Item* it = GetItem(taskId))
	{
		for (size_t i = 0; i < MAX_COEFS; ++i)
			it->progress[i] = factors[i];

		it->isReady = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void ProgressManager::SetDone(unsigned taskId, bool oldest)
{
	thread::Lock lock(m_CS);

	if (m_Count)
	{
		// Пометим указанное задание как завершённое
		const auto firstId = m_Items[m_Head].taskId;
		if (taskId >= firstId && taskId < static_cast<uint64_t>(firstId) + m_Count)
		{
			unsigned idx = m_Head + taskId - firstId;
			if (idx >= MAX_TASKS)
				idx -= MAX_TASKS;

			m_Items[idx].isDone = true;
		}

		// Удалим из буфера задания, id которых <= taskId (если oldest == true, то есть указанный
		// taskId является самым старым заданием), а также все "готовые" задания в начале буфера
		for (; m_Count; --m_Count)
		{
			Item& it = m_Items[m_Head];
			if (!it.isDone && (!oldest || taskId < it.taskId))
				break;

			// Если были данные прогресса, сохраним их
			if (it.isReady)
			{
				for (size_t i = 0; i < MAX_COEFS; ++i)
					m_Progress[i] = it.progress[i];

				m_IsReady = true;
			}

			auto next = m_Head + 1;
			m_Head = (next < MAX_TASKS) ? next : 0;
		}
	}

	// Для того, чтобы вывод прогресса был последовательным, в буфере всегда должно оставаться
	// хотя бы одно задание. Если буфер оказался пуст, то добавим в него завершённое задание
	if (!m_Count && oldest)
	{
		m_Count = 1;
		m_Items[m_Head].Reset(taskId);
		m_Items[m_Head].isDone = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
ProgressManager::Item* ProgressManager::GetItem(unsigned taskId)
{
	// Если буфер пуст или значение taskId меньше наименьшего значения в буфере
	// (произошло переполнение счётчика), то очистим буфер (сбросим состояние)
	const auto firstId = m_Items[m_Head].taskId;
	if (!m_Count || taskId < firstId)
	{
		// Иногда возникает ситуация, когда при пустом буфере один из рабочих потоков вызывает функцию
		// SetProgress, а затем это делает другой поток для задания с более низким taskId. Это может привести
		// к кратковременному нарушению порядка вывода прогресса. Поэтому, если эта ситуация произошла не в
		// результате переполнения счётчика, мы будем игнорировать задания с более низким taskId
		if (m_Count && (firstId < 0xffff0000 || taskId > 0x10000))
			return nullptr;

		m_Count = 1;
		m_Items[m_Head].Reset(taskId);

		return m_Items;
	}

	if (taskId >= static_cast<uint64_t>(firstId) + m_Count)
	{
		// Если в буфере всего 1 задание и оно завершено, то заменим его. Эта ситуация возникает
		// только тогда, когда задание было "оставлено" лишь для сохранения порядка следования
		if (m_Count == 1 && m_Items[m_Head].isDone)
		{
			m_Items[m_Head].Reset(taskId);
			return &m_Items[m_Head];
		}

		// Если расстояние между id больше размера буфера, то проигнорируем прогресс
		// этого задания: сейчас это задание достаточно далеко от текущего прогресса
		const unsigned newCount = taskId - firstId + 1;
		if (newCount > MAX_TASKS)
			return nullptr;

		for (unsigned i = m_Count; i < newCount; ++i)
		{
			unsigned idx = m_Head + i;
			if (idx >= MAX_TASKS)
				idx -= MAX_TASKS;

			m_Items[idx].Reset(firstId + i);
		}

		m_Count = newCount;
	}

	auto idx = m_Head + taskId - firstId;
	return &m_Items[(idx < MAX_TASKS) ? idx : idx - MAX_TASKS];
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ProgressManager::Item
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
void ProgressManager::Item::Reset(unsigned id)
{
	taskId = id;
	isReady = false;
	isDone = false;
}
