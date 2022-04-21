//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "searchbase.h"
#include "solution.h"

#include <core/platform.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
struct WorkerTask final
{
	// Рабочие потоки обычно не перебирают коэффициенты левой части уравнения (они все обычно заданы
	// заданием). Однако задание может задавать вместе со всеми коэффициентами левой части и первые
	// коэффициенты правой. Суммарное количество заданных коэффициентов в любом случае не может
	// превышать максимально возможного количества коэффициентов в левой части
	static constexpr int MAX_FACTOR_COUNT = SearchBase::MAX_FACTOR_COUNT / 2;

	uint64_t id = 0;					// Уникальный номер задания
	int factorCount = 0;				// Количество заданных коэффициентов
	unsigned factors[MAX_FACTOR_COUNT];	// Заданные значения первых коэффициентов

	WorkerTask& operator =(const WorkerTask& that) noexcept;

	bool operator ==(const WorkerTask& rhs) const;
	bool operator <(const WorkerTask& rhs) const;

	// Сравнивает коэффициенты задания с коэффициентами указанного решения.
	// Возвращает true, если коэффициенты задания "больше" указанного решения
	bool operator >(const Solution& rhs) const { return rhs.IsLower(factors, factorCount); }
};

//--------------------------------------------------------------------------------------------------------------------------------
class TaskList final
{
	AML_NONCOPYABLE(TaskList)

public:
	TaskList();
	~TaskList();

	bool IsEmpty() const noexcept { return !m_Size; }
	size_t GetSize() const noexcept { return m_Size; }

	void Clear() noexcept { m_Size = 0; }

	// Возвращает указатель на первое задание в буфере.
	// Если буфер пуст, то функция вернёт nullptr
	WorkerTask* GetFront() noexcept;

	// Удаляет из буфера count первых заданий. Если заданий в
	// буфере меньше, чем count, то буфер очищается полностью
	void PopFront(size_t count = 1) noexcept;

	// Выделяет в буфере новое (пустое) задание, никак не инициализируя
	// поля структуры WorkerTask, и помещает это задание в конец списка
	WorkerTask* Allocate();

private:
	void Grow(size_t newCapacity);

	WorkerTask* m_Buffer = nullptr;	// Кольцевой буфер заданий
	size_t m_Capacity = 0;			// Размер буфера m_Buffer (кол-во заданий)
	size_t m_Size = 0;				// Текущее количество заданий в буфере
	size_t m_Head = 0;				// Индекс самого первого задания
};
