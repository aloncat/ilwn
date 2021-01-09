//∙MDPN
#pragma once

#include "number.h"

#include <core/platform.h>
#include <core/util.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   NumberSet - хеш-таблица (набор) чисел FixNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class NumberSet
{
	AML_NONCOPYABLE(NumberSet)

public:
	NumberSet();
	~NumberSet();

	void Clear(bool freeMem = true);
	// Возвращает количество задействованных элементов набора
	size_t GetSize() const { return m_TCount + m_CCount - m_RCount; }

	// Возвращает true, если число num содержится в наборе
	bool Exists(const Number& num) const;
	bool Exists(const FixNumber& num) const;

	// Добавляет число num в набор, если его в наборе ещё нет. Возвращает true, если число было добавлено, и
	// false, если такое число в наборе уже есть. Число 0 никогда не добавляется, это связано с особенностью
	// работы контейнера. Весь набор чисел разбит на 8 частей. Если размер всего набора достиг своего макс.
	// значения, то будет удалён первый блок самой крупной части элементов. Если размер какой-либо из частей
	// достиг 1/5 (при CLEAR_GAIN == 10) максимального размера набора, то будет удалён первый блок этой
	// части. Новое число всегда добавляется после удаления элементов (если они удалялись)
	bool Insert(const Number& num) { return Insert(FixNumber(num)); }
	bool Insert(const FixNumber& num);

protected:
	static constexpr size_t HASH_BITS = 24;			// Кол-во бит хеша, определяет размер набора (24 -> 1200 MiB)
	static constexpr size_t EIGHTH_CHUNK_C = 16;	// Кол-во блоков по CHUNK_SIZE элементов для одной части

	// Количество блоков в расчёте на одну часть, при котором происходит удаление элементов. Значение должно
	// лежать в пределах от 2 до (EIGHTH_CHUNK_C - 1). Чем оно будет выше, тем выше будет среднее количество
	// элементов на один хеш. Более высокие значения позволяют задействовать больше памяти и увеличить
	// ёмкость набора, но вместе с этим замедляют работу функции Exists
	static constexpr size_t CLEAR_GAIN = 10;

	// При значениях EIGHTH_CHUNK_C (16), CLEAR_GAIN (10) и CHUNK_SIZE (HB-5), на каждый хеш приходится в
	// среднем по 3.5 элемента. Это обеспечивает наилучший баланс между скоростью работы функции Exists и
	// размером хеш-таблицы (26.7% от максимального объёма расходуемой памяти). Значение (HB-6) немного
	// увеличивает скорость работы (в среднем 2.25 элемента на хеш), значение (HB-4) приводит к
	// значительному снижению скорости работы (в среднем 6 элементов на хеш)
	static constexpr size_t CHUNK_SIZE = 1 << (HASH_BITS - 5);

	struct Item {
		static constexpr uint32_t LAST = ~0u;

		FixNumber num;			// Число (0 если элемент не задействован)
		uint32_t next = LAST;	// Индекс следующего числа в m_ChunkA (LAST если этот элемент последний)

		void Reset() { num.SetZero(); next = LAST; }
	};

	void RaiseTable();
	void Purge(size_t eighth);
	uint32_t GetNext(size_t eighth);
	template<class T> unsigned GetHash(const T& num) const;

	Item* AllocateMem(size_t itemC);
	void FreeMem(Item*& pBlock);

	Item* GetItem(uint32_t i) const { return &m_ChunkA[i / CHUNK_SIZE][i & (CHUNK_SIZE - 1)]; }

	Item* m_TableA = nullptr;	// Хеш-таблица (первые элементы цепочек или "0")
	Item** m_ChunkA = nullptr;	// Массив блоков по CHUNK_SIZE распределяемых элементов
	size_t m_NextA[8];			// Текущие индексы новых элементов каждой из частей
	size_t m_TCount = 0;		// Количество задействованных элементов в m_TableA
	size_t m_CCount = 0;		// Количество задействованных элементов m_ChunkA
	size_t m_RCount = 0;		// Суммарное кол-во элементов, перенесённых из блоков в m_TableA
	size_t m_RCountA[8];		// Кол-во эл-в, перенесённых из блоков в m_TableA для каждой из частей
	unsigned m_HBits = 0;		// Количество используемых бит хеша в данный момент
	unsigned m_Purge = 0;		// Части (мл. 8 бит), достигшие крит. размера
};
