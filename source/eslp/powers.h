//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "uint128.h"

#include <core/debug.h>
#include <core/platform.h>
#include <core/util.h>

#include <algorithm>
#include <math.h>
#include <type_traits>

//--------------------------------------------------------------------------------------------------------------------------------
class PowersBase
{
	AML_NONCOPYABLE(PowersBase)

public:
	int GetPower() const
	{
		return m_Power;
	}

	unsigned GetUpperValue() const
	{
		return m_UpperValue;
	}

protected:
	PowersBase() = default;

protected:
	int m_Power = 0;			// Значение степени, от 1 до 32
	unsigned m_UpperValue = 0;	// Наибольшее корректное значение
};

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class Powers : public PowersBase
{
public:
	~Powers()
	{
		AML_SAFE_DELETEA(m_Powers);
	}

	unsigned Init(int power, int factor = 1, unsigned upperValue = ~0u)
	{
		upperValue = std::min(upperValue, CalcUpperValue(power, factor));

		m_Power = power;
		m_UpperValue = upperValue;

		AML_SAFE_DELETEA(m_Powers);
		m_Powers = new T[upperValue + 1];

		if (power == 1)
		{
			for (unsigned i = 0; i <= upperValue; ++i)
				m_Powers[i] = i;
		} else
		{
			m_Powers[0] = 0;
			for (unsigned i = 1; i <= upperValue; ++i)
				m_Powers[i] = CalcPower(i, power);
		}

		return upperValue;
	}

	const T* GetData() const
	{
		return m_Powers;
	}

	const T& operator [](size_t index) const
	{
		return m_Powers[index];
	}

	static T CalcPower(unsigned value, int power)
	{
		uint64_t v2 = value;
		T p = v2 *= value;

		for (int i = 2; i + 2 <= power; i += 2)
			p *= v2;

		return (power & 1) ? p * value : p;
	}

	static unsigned CalcUpperValue(int power, int factor = 1)
	{
		static_assert(std::is_same_v<T, uint64_t> || std::is_same_v<T, UInt128>,
			"Only uint64_t or UInt128 types are supported");

		if (!Verify(power >= 1 && power <= 32 && factor >= 1))
			return 0;

		// NB: максимальное значение ограничено размером массива в 1 гигабайт
		const unsigned upperBound = static_cast<unsigned>((1024 * 1024 * 1024) / sizeof(T)) - 1;

		if (power == 1)
		{
			return std::min(upperBound, UINT_MAX / factor);
		}

		// Макс. значения для T
		constexpr double highest[] = { 0,
			1.8446744073709552e+19,		// 64 бита (для uint64_t)
			3.4028236692093846e+38		// 128 бит (для UInt128)
		};

		constexpr double high = highest[sizeof(T) / 8];
		const auto uv = static_cast<unsigned>(std::min(pow(high / factor, 1.0 / power),
			static_cast<double>(upperBound)));

		if (uv)
		{
			T o, p = 1;
			for (int i = 0; i < power; ++i)
			{
				o = p;
				p *= uv;

				// Проверяем, не возникло ли переполнение. Если оно возникло, значит значение uv в степени
				// power, умноженное на factor, не поместится в T, и нам нужно значение на единицу меньше
				if ((p * factor) / uv / factor != o)
					return uv - 1;
			}
		}

		return uv;
	}

protected:
	T* m_Powers = nullptr;
};
