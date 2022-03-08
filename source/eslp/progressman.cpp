//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "progressman.h"

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

	const unsigned* progress = nullptr;

	if (m_Count)
	{
		// Если у самого старого незавершённого задания (голова
		// списка) есть данные прогресса, то вернём именно их
		if (const Item& it = m_Items[m_Head]; it.isReady)
			progress = it.progress;
	}

	// В ином случае, вернём данные последнего завершённого задания
	if (!progress && m_IsReady)
		progress = m_Progress;

	if (progress)
	{
		for (size_t i = 0; i < MAX_COEFS; ++i)
			factors[i] = progress[i];

		return true;
	}

	return false;
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
void ProgressManager::SetDone(unsigned taskId)
{
	thread::Lock lock(m_CS);

	// Удалим из буфера задания, id которых <= taskId
	while (m_Count && taskId >= m_Items[m_Head].taskId)
	{
		// Если были данные прогресса, сохраним их
		if (Item& it = m_Items[m_Head]; it.isReady)
		{
			for (size_t i = 0; i < MAX_COEFS; ++i)
				m_Progress[i] = it.progress[i];

			m_IsReady = true;
		}

		auto next = m_Head + 1;
		m_Head = (next < MAX_TASKS) ? next : 0;
		--m_Count;
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
		// NB: иногда возникает ситуация, когда при пустом буфере один из рабочих потоков вызывает функцию
		// SetProgress, а затем это делает другой поток для задания с более низким taskId. Это приводит к тому,
		// что на какой-то момент прогресс отображается некорректно. Поэтому, если эта ситуация произошла не в
		// результате переполнения счётчика, мы будем игнорировать задания с более низким taskId
		if (m_Count && firstId < 0xffff0000)
			return nullptr;

		m_Count = 1;
		m_Head = 0;

		m_Items[0].taskId = taskId;
		m_Items[0].isReady = false;

		return &m_Items[0];
	}

	if (taskId >= static_cast<uint64_t>(firstId) + m_Count)
	{
		// Если расстояние между id больше размера буфера, то игнорируем задание
		const unsigned newCount = taskId - firstId + 1;
		if (newCount > MAX_TASKS)
			return nullptr;

		unsigned head = m_Head + m_Count;
		for (unsigned i = m_Count; i < newCount; ++i)
		{
			if (head >= MAX_TASKS)
				head -= MAX_TASKS;

			Item& it = m_Items[head++];

			it.taskId = firstId + i;
			it.isReady = false;
		}

		m_Count = newCount;
	}

	auto idx = m_Head + taskId - firstId;
	return &m_Items[(idx < MAX_TASKS) ? idx : idx - MAX_TASKS];
}
