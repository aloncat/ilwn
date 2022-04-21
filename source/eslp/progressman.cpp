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
	m_StallId = 0;
	m_IsReady = false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool ProgressManager::GetProgress(unsigned* factors)
{
	thread::Lock lock(m_CS);

	const unsigned* progress = nullptr;
	// Если у самого старого незавершённого задания (в
	// голове списка) есть данные, то вернём именно их
	if (m_Count && m_Items[m_Head].isReady)
	{
		m_StallId = 0;
		progress = m_Items[m_Head].progress;
	} else
	{
		if (m_Count)
		{
			// Если такого задания нет, то проверим, нет ли "зависшего" задания, то есть такого, для которого событие
			// завершения не было обработано из-за того, что соответствующего элемента не было в буфере, но потом при
			// обновлении прогресса другого задания такой элемент был вставлен в список (и теперь он "повис")
			if (m_StallId && m_StallId == m_Items[m_Head].taskId)
			{
				while (m_Count && !m_Items[m_Head].isReady)
				{
					auto next = m_Head + 1;
					m_Head = (next < MAX_TASKS) ? next : 0;
					--m_Count;
				}

				if (m_Count && m_Items[m_Head].isReady)
				{
					m_StallId = 0;
					progress = m_Items[m_Head].progress;
				}
			} else
			{
				// Запомним ID "зависшего" задания
				m_StallId = m_Items[m_Head].taskId;
			}
		}

		// Вернём данные последнего завершённого задания,
		// если ничего другого для вывода сейчас нет
		if (!progress)
		{
			if (m_IsReady)
				progress = m_Progress;
			else
				return false;
		}
	}

	for (size_t i = 0; i < MAX_COEFS; ++i)
		factors[i] = progress[i];

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void ProgressManager::SetProgress(const unsigned* factors, uint64_t taskId)
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
void ProgressManager::SetDone(uint64_t taskId, bool oldest)
{
	thread::Lock lock(m_CS);

	if (m_Count)
	{
		// Пометим указанное задание как завершённое
		const auto firstId = m_Items[m_Head].taskId;
		if (taskId >= firstId && taskId < firstId + m_Count)
		{
			auto idx = m_Head + taskId - firstId;
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
	if (!m_Count)
	{
		m_Count = 1;
		m_Items[m_Head].Reset(taskId);
		m_Items[m_Head].isDone = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
ProgressManager::Item* ProgressManager::GetItem(uint64_t taskId)
{
	if (!m_Count)
	{
		m_Count = 1;
		m_Items[m_Head].Reset(taskId);
		return m_Items + m_Head;
	}

	const auto firstId = m_Items[m_Head].taskId;
	// Иногда возникает ситуация, когда при пустом буфере один из рабочих потоков вызывает
	// функцию SetProgress, а затем это делает другой поток для задания с меньшим taskId.
	// Это может привести к кратковременному нарушению порядка вывода прогресса. Поэтому
	// в такой ситуации мы будем игнорировать задания с меньшими taskId
	if (taskId < firstId)
		return nullptr;

	if (taskId >= firstId + m_Count)
	{
		// Если самое первое задание в буфере завершено, не имеет прогресса и находится
		// непосредственно перед текущим, то удалим его. Эта ситуация возникает только
		// тогда, когда задание было "оставлено" лишь для сохранения порядка следования
		if (Item it = m_Items[m_Head]; it.isDone && !it.isReady && it.taskId + 1 == taskId)
		{
			auto next = m_Head + 1;
			m_Head = (next < MAX_TASKS) ? next : 0;
			m_Items[m_Head].Reset(taskId);
			return m_Items + m_Head;
		}

		// Если расстояние между id больше размера буфера, то проигнорируем прогресс
		// этого задания: сейчас это задание достаточно далеко от текущего прогресса
		const unsigned newCount = static_cast<unsigned>(taskId - firstId + 1);
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
void ProgressManager::Item::Reset(uint64_t id)
{
	taskId = id;
	isReady = false;
	isDone = false;
}
