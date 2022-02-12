//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/platform.h>

#include <intrin.h>

//--------------------------------------------------------------------------------------------------------------------------------
struct UInt128
{
	uint64_t loPart;	// Младшие 64 бита числа
	uint64_t hiPart;	// Старшие 64 бита числа

public:
	UInt128() = default;

	UInt128(uint64_t value) noexcept
		: loPart(value)
		, hiPart(0)
	{
	}

	UInt128& operator =(uint64_t value) noexcept
	{
		loPart = value;
		hiPart = 0;

		return *this;
	}

	UInt128 operator +(uint64_t rhs) const noexcept
	{
		UInt128 result;

		unsigned char c;
		c = _addcarry_u64(0, loPart, rhs, &result.loPart);
		_addcarry_u64(c, hiPart, 0, &result.hiPart);

		return result;
	}

	UInt128 operator +(const UInt128& rhs) const noexcept
	{
		UInt128 result;

		unsigned char c;
		c = _addcarry_u64(0, loPart, rhs.loPart, &result.loPart);
		_addcarry_u64(c, hiPart, rhs.hiPart, &result.hiPart);

		return result;
	}

	UInt128& operator +=(uint64_t rhs) noexcept
	{
		unsigned char c;
		c = _addcarry_u64(0, loPart, rhs, &loPart);
		_addcarry_u64(c, hiPart, 0, &hiPart);

		return *this;
	}

	UInt128& operator +=(const UInt128& rhs) noexcept
	{
		unsigned char c;
		c = _addcarry_u64(0, loPart, rhs.loPart, &loPart);
		_addcarry_u64(c, hiPart, rhs.hiPart, &hiPart);

		return *this;
	}

	UInt128 operator -(uint64_t rhs) const noexcept
	{
		UInt128 result;

		unsigned char c;
		c = _subborrow_u64(0, loPart, rhs, &result.loPart);
		_subborrow_u64(c, hiPart, 0, &result.hiPart);

		return result;
	}

	UInt128 operator -(const UInt128& rhs) const noexcept
	{
		UInt128 result;

		unsigned char c;
		c = _subborrow_u64(0, loPart, rhs.loPart, &result.loPart);
		_subborrow_u64(c, hiPart, rhs.hiPart, &result.hiPart);

		return result;
	}

	UInt128& operator -=(uint64_t rhs) noexcept
	{
		unsigned char c;
		c = _subborrow_u64(0, loPart, rhs, &loPart);
		_subborrow_u64(c, hiPart, 0, &hiPart);

		return *this;
	}

	UInt128& operator -=(const UInt128& rhs) noexcept
	{
		unsigned char c;
		c = _subborrow_u64(0, loPart, rhs.loPart, &loPart);
		_subborrow_u64(c, hiPart, rhs.hiPart, &hiPart);

		return *this;
	}

	UInt128 operator *(uint64_t rhs) const noexcept
	{
		UInt128 result;

		uint64_t hi, t;
		result.loPart = _umul128(rhs, loPart, &hi);
		result.hiPart = hi + _umul128(rhs, hiPart, &t);

		return result;
	}

	UInt128& operator *=(uint64_t rhs) noexcept
	{
		uint64_t hi, t;
		loPart = _umul128(rhs, loPart, &hi);
		hiPart = hi + _umul128(rhs, hiPart, &t);

		return *this;
	}

	UInt128 operator /(uint32_t rhs) const;
	//UInt128& operator /=(uint32_t rhs);

	uint32_t operator %(uint32_t rhs) const;
	//UInt128& operator %=(uint32_t rhs);

	//UInt128 operator <<(size_t bits) const noexcept
	//UInt128& operator <<=(size_t bits) noexcept

	//UInt128 operator >>(size_t bits) const noexcept
	//UInt128& operator >>=(size_t bits) noexcept

	bool operator ==(uint64_t rhs) const noexcept
	{
		return loPart == rhs && !hiPart;
	}

	bool operator ==(const UInt128& rhs) const noexcept
	{
		return loPart == rhs.loPart && hiPart == rhs.hiPart;
	}

	bool operator !=(uint64_t rhs) const noexcept
	{
		return loPart != rhs || hiPart;
	}

	bool operator !=(const UInt128& rhs) const noexcept
	{
		return loPart != rhs.loPart || hiPart != rhs.hiPart;
	}

	bool operator <(uint64_t rhs) const noexcept
	{
		return !hiPart && loPart < rhs;
	}

	bool operator <(const UInt128& rhs) const noexcept
	{
		return hiPart < rhs.hiPart ||
			(hiPart == rhs.hiPart && loPart < rhs.loPart);
	}

	//bool operator <=(uint64_t rhs) const noexcept
	//bool operator <=(const UInt128& rhs) const noexcept

	bool operator >(uint64_t rhs) const noexcept
	{
		return hiPart || loPart > rhs;
	}

	bool operator >(const UInt128& rhs) const noexcept
	{
		return hiPart > rhs.hiPart ||
			(hiPart == rhs.hiPart && loPart > rhs.loPart);
	}

	//bool operator >=(uint64_t rhs) const noexcept
	//bool operator >=(const UInt128& rhs) const noexcept

protected:
	static void Divide32(uint32_t* dividend, uint32_t divisor, uint32_t* rest) noexcept;
};
