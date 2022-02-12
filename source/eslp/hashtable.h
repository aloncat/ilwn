//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "uint128.h"

#include <core/platform.h>
#include <core/util.h>

#include <algorithm>
#include <string.h>

//--------------------------------------------------------------------------------------------------------------------------------
class HashTable
{
	AML_NONCOPYABLE(HashTable)

public:
	HashTable() = default;

	~HashTable()
	{
		AML_SAFE_DELETEA(m_Table);
	}

	template<class T>
	void Init(unsigned upperValue, const T& powers)
	{
		AML_SAFE_DELETEA(m_Table);
		m_Table = new uint8_t[ARRAY_SIZE];
		memset(m_Table, 0, ARRAY_SIZE);

		const auto high = std::min(upperValue, powers.GetUpperValue());

		for (unsigned i = 1; i <= high; ++i)
		{
			auto hash = GetHash(powers[i]);
			m_Table[hash >> 3] |= 1 << (hash & 7);
		}
	}

	bool Exists(uint64_t value) const
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 3] & (1 << (hash & 7));
	}

	bool Exists(const UInt128& value) const
	{
		auto hash = GetHash(value);
		return m_Table[hash >> 3] & (1 << (hash & 7));
	}

protected:
	static constexpr unsigned HASH_BITS = 18;
	static constexpr unsigned BYTE_MASK = (1 << HASH_BITS) - 1;
	static constexpr unsigned ARRAY_SIZE = 1 << (HASH_BITS - 3);

	static unsigned GetHash(uint64_t value)
	{
		return value & BYTE_MASK;
	}

	static unsigned GetHash(const UInt128& value)
	{
		return value.loPart & BYTE_MASK;
	}

protected:
	uint8_t* m_Table = nullptr;
};
