//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "workertask.h"

#include <core/debug.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   WorkerTask
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
WorkerTask& WorkerTask::operator =(const WorkerTask& that) noexcept
{
	if (this != &that)
	{
		id = that.id;
		proof = 0;

		factorCount = that.factorCount;

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
bool WorkerTask::operator ==(const WorkerTask& rhs) const
{
	if (Verify(factorCount && factorCount == rhs.factorCount))
	{
		for (size_t i = 0; i < factorCount; ++i)
		{
			if (factors[i] != rhs.factors[i])
				return false;
		}

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool WorkerTask::operator <(const WorkerTask& rhs) const
{
	if (Verify(factorCount && factorCount == rhs.factorCount))
	{
		for (size_t i = 0; i < factorCount; ++i)
		{
			if (factors[i] != rhs.factors[i])
			{
				return factors[i] < rhs.factors[i];
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TaskList
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
TaskList::TaskList()
	: m_Capacity(24)
{
	m_Buffer = new WorkerTask[m_Capacity];
}

//--------------------------------------------------------------------------------------------------------------------------------
TaskList::~TaskList()
{
	AML_SAFE_DELETEA(m_Buffer);
}

//--------------------------------------------------------------------------------------------------------------------------------
WorkerTask* TaskList::GetFront() noexcept
{
	return m_Size ? m_Buffer + m_Head : nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------
void TaskList::PopFront(size_t count) noexcept
{
	if (count > m_Size)
		count = m_Size;

	m_Head += count;
	if (m_Head >= m_Capacity)
		m_Head -= m_Capacity;

	m_Size -= count;
}

//--------------------------------------------------------------------------------------------------------------------------------
WorkerTask* TaskList::Allocate()
{
	if (m_Size >= m_Capacity)
	{
		// Количество заданий в буфере зависит от количеста рабочих потоков. При параллельном выполнении заданий
		// периодически возникает ситуация, когда несколько заданий из середины буфера уже завершены, но самое
		// первое задание ещё выполняется, поэтому ничего не удаляется. Обычно при таких условиях максимальное
		// количество заданий в буфере не превышает удвоенного количества рабочих потоков. Но для экономии
		// памяти будем увеличивать размер буфера порциями по 16 заданий
		Grow(m_Capacity + 16);
	}

	size_t i = m_Head + m_Size;
	if (i >= m_Capacity)
		i -= m_Capacity;

	++m_Size;
	return m_Buffer + i;
}

//--------------------------------------------------------------------------------------------------------------------------------
void TaskList::Grow(size_t newCapacity)
{
	if (newCapacity > m_Capacity)
	{
		WorkerTask* old = m_Buffer;
		m_Buffer = new WorkerTask[newCapacity];

		for (size_t i = 0, k = m_Head; i < m_Size; ++i)
		{
			m_Buffer[i] = old[k];
			if (++k == m_Capacity)
				k = 0;
		}

		m_Capacity = newCapacity;
		m_Head = 0;
	}
}
