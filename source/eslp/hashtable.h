//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "uint128.h"

#include <core/platform.h>
#include <core/util.h>

#include <algorithm>
#include <string.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   HashTable - хеш-таблица значений степеней для всех алгоритмов поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<size_t HASH_BITS>
class HashTable
{
	AML_NONCOPYABLE(HashTable)

	// Длина хеша: от 17 бит (размер массива 16 KiB) до 28 бит (32 MiB)
	static_assert(HASH_BITS >= 17 && HASH_BITS <= 28, "Invalid hash length");

public:
	HashTable() = default;

	~HashTable()
	{
		AML_SAFE_DELETEA(m_Table);
	}

	template<class T>
	void Init(unsigned upperValue, const T& powers)
	{
		if (!m_Table)
		{
			m_Table = new uint8_t[ARRAY_SIZE];
		}
		memset(m_Table, 0, ARRAY_SIZE);

		const auto high = std::min(upperValue, powers.GetUpperValue());

		for (unsigned i = 1; i <= high; ++i)
		{
			auto hash = GetHash(powers[i]);
			m_Table[hash >> 3] |= 1 << (hash & 7);
		}
	}

	template<class NumberT>
	bool Exists(const NumberT& value) const noexcept
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 3] & (1 << (hash & 7));
	}

protected:
	static constexpr unsigned BIT_MASK = (1 << HASH_BITS) - 1;
	static constexpr unsigned ARRAY_SIZE = 1 << (HASH_BITS - 3);

	static unsigned GetHash(uint64_t value) noexcept
	{
		return value & BIT_MASK;
	}

	static unsigned GetHash(const UInt128& value) noexcept
	{
		return value.loPart & BIT_MASK;
	}

protected:
	uint8_t* m_Table = nullptr;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SuperHashTable - большая хеш-таблица значений для экспериментальных алгоритмов поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<size_t HASH_BITS>
class SuperHashTable
{
	AML_NONCOPYABLE(SuperHashTable)

	// Длина хеша: от 17 бит (размер массива 16 KiB) до 35 бит (4 GiB)
	static_assert(HASH_BITS >= 17 && HASH_BITS <= 35, "Invalid hash length");

public:
	SuperHashTable() = default;

	~SuperHashTable()
	{
		AML_SAFE_DELETEA(m_Table);
	}

	void Init()
	{
		if (!m_Table)
		{
			m_Table = new uint32_t[ARRAY_SIZE];
		}
		memset(m_Table, 0, 4 * ARRAY_SIZE);
	}

	template<class NumberT>
	bool Exists(const NumberT& value) const noexcept
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 5] & (1 << (hash & 31));
	}

	template<class NumberT>
	void Insert(const NumberT& value) noexcept
	{
		auto hash = GetHash(value);
		m_Table[hash >> 5] |= 1 << (hash & 31);
	}

protected:
	static constexpr uint64_t BIT_MASK = (1llu << HASH_BITS) - 1;
	static constexpr size_t ARRAY_SIZE = 1 << (HASH_BITS - 5);

	static uint64_t GetHash(uint64_t value) noexcept
	{
		return value & BIT_MASK;
	}

	static uint64_t GetHash(const UInt128& value) noexcept
	{
		return value.loPart & BIT_MASK;
	}

protected:
	uint32_t* m_Table = nullptr;
};
