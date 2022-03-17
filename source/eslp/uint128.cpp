//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "uint128.h"

#include <core/exception.h>

//--------------------------------------------------------------------------------------------------------------------------------
UInt128 UInt128::operator /(uint32_t rhs) const
{
	UInt128 result = *this;

	if (rhs > 1)
	{
		uint32_t rest = 0;
		uint32_t* number = reinterpret_cast<uint32_t*>(&result);

		Divide32(number + 3, rhs, &rest);
		Divide32(number + 2, rhs, &rest);
		Divide32(number + 1, rhs, &rest);
		Divide32(number + 0, rhs, &rest);
	}
	else if (!rhs)
	{
		throw util::ELogic("Division by zero");
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
UInt128& UInt128::operator /=(uint32_t rhs)
{
	if (rhs > 1)
	{
		uint32_t rest = 0;
		uint32_t* number = reinterpret_cast<uint32_t*>(this);

		Divide32(number + 3, rhs, &rest);
		Divide32(number + 2, rhs, &rest);
		Divide32(number + 1, rhs, &rest);
		Divide32(number + 0, rhs, &rest);
	}
	else if (!rhs)
	{
		throw util::ELogic("Division by zero");
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
uint32_t UInt128::operator %(uint32_t rhs) const
{
	if (rhs > 1)
	{
		uint32_t rest = 0;
		UInt128 value = *this;
		uint32_t* number = reinterpret_cast<uint32_t*>(&value);

		Divide32(number + 3, rhs, &rest);
		Divide32(number + 2, rhs, &rest);
		Divide32(number + 1, rhs, &rest);
		Divide32(number + 0, rhs, &rest);

		return rest;
	}
	else if (!rhs)
	{
		throw util::ELogic("Division by zero");
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
inline void UInt128::Divide32(uint32_t* dividend, uint32_t divisor, uint32_t* rest) noexcept
{
	uint64_t u64Dividend = *rest;
	u64Dividend = (u64Dividend << 32) + *dividend;
	*dividend = static_cast<uint32_t>(u64Dividend / divisor);
	*rest = static_cast<uint32_t>(u64Dividend % divisor);
}
