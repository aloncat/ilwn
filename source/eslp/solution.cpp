//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "solution.h"

#include <core/array.h>
#include <core/debug.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Solution
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
Solution::Solution(const unsigned* allFactors, int leftCount, int rightCount)
{
	Assert(leftCount > 0 && rightCount > 0);

	m_LeftCount = leftCount;
	m_Factors.reserve(leftCount + rightCount);

	for (int i = 0; i < leftCount; ++i)
		m_Factors.push_back(allFactors[i]);

	allFactors += leftCount;
	for (int i = 0; i < rightCount; ++i)
		m_Factors.push_back(allFactors[i]);
}

//--------------------------------------------------------------------------------------------------------------------------------
Solution::Solution(const unsigned* leftFactors, const unsigned* rightFactors, int leftCount, int rightCount)
{
	Assert(leftCount > 0 && rightCount > 0);

	m_LeftCount = leftCount;
	m_Factors.reserve(leftCount + rightCount);

	for (int i = 0; i < leftCount; ++i)
		m_Factors.push_back(leftFactors[i]);

	for (int i = 0; i < rightCount; ++i)
		m_Factors.push_back(rightFactors[i]);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Solution::SortFactors()
{
	if (!m_Factors.empty())
	{
		if (m_LeftCount > 1)
		{
			std::sort(m_Factors.begin(), m_Factors.begin() + m_LeftCount, [](unsigned lhs, unsigned rhs) {
				return lhs > rhs;
			});
		}

		std::sort(m_Factors.begin() + m_LeftCount, m_Factors.end(), [](unsigned lhs, unsigned rhs) {
			return lhs > rhs;
		});
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Solution::Swap(Solution& that)
{
	if (this != &that)
	{
		int t = m_LeftCount;
		m_LeftCount = that.m_LeftCount;
		that.m_LeftCount = t;

		m_Factors.swap(that.m_Factors);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::IsConfluent() const
{
	auto factors = m_Factors.data();
	const auto count = m_Factors.size();

	for (int i = 0; i < m_LeftCount; ++i)
	{
		for (size_t j = m_LeftCount; j < count; ++j)
		{
			if (factors[i] == factors[j])
				return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::operator ==(const Solution& rhs) const
{
	if (size_t count = m_Factors.size(); Verify(count && count == rhs.m_Factors.size()))
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (m_Factors[i] != rhs.m_Factors[i])
				return false;
		}

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::operator <(const Solution& rhs) const
{
	if (size_t count = m_Factors.size(); Verify(count && count == rhs.m_Factors.size()))
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (m_Factors[i] != rhs.m_Factors[i])
			{
				return m_Factors[i] < rhs.m_Factors[i];
			}
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::IsLower(const unsigned* factors, int count) const
{
	if (Verify(count <= m_Factors.size()))
	{
		for (int i = 0; i < count; ++i)
		{
			if (auto f = m_Factors[i]; f != factors[i])
				return f < factors[i];
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Solutions
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
bool Solutions::Insert(const Solution& solution, bool verify)
{
	if (verify && !IsPrimitive(solution))
		return false;

	Solution sorted = solution;
	sorted.SortFactors();

	auto result = m_Solutions.insert(std::move(sorted));
	return result.second;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solutions::Insert(Solution&& solution, bool verify)
{
	if (verify && !IsPrimitive(solution))
		return false;

	solution.SortFactors();

	auto result = m_Solutions.insert(std::move(solution));
	return result.second;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Solutions::Prune(const unsigned* factors, int count)
{
	if (m_Solutions.size() > 10000)
	{
		for (auto it = m_Solutions.begin(); it != m_Solutions.end();)
		{
			if (it->IsLower(factors, count))
				it = m_Solutions.erase(it);
			else
				++it;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solutions::IsPrimitive(const Solution& solution)
{
	const int leftCount = solution.GetLCount();
	const int rightCount = solution.GetRCount();

	if (!Verify(leftCount > 0 && rightCount > 0))
		return false;

	const int total = leftCount + rightCount;
	auto allFactors = solution.GetLFactors();

	unsigned lowest = UINT_MAX;
	for (int i = 0; i < total; ++i)
	{
		auto f = allFactors[i];
		lowest = (f < lowest) ? f : lowest;
	}

	m_Factors.clear();
	GetFactors(m_Factors, lowest);

	for (unsigned p : m_Factors)
	{
		bool allDivisible = true;
		for (int i = 0; i < total; ++i)
		{
			if (allFactors[i] % p)
			{
				allDivisible = false;
				break;
			}
		}

		if (allDivisible)
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Solutions::GetFactors(std::vector<unsigned>& factors, unsigned value)
{
	if (value > 1)
	{
		if (m_Primes.empty())
		{
			// Для максимального значения value 2^32-1 достаточно проверки делимости на простые
			// числа, не превышающие округлённого вниз до целого корня квадратного из этого значения
			m_Primes = GetPrimes(65535);
		}

		const unsigned count = static_cast<unsigned>(m_Primes.size());
		const unsigned limit = static_cast<unsigned>(sqrt(value));

		for (unsigned lastPrime = 0, i = 0; i < count;)
		{
			const unsigned prime = m_Primes[i];
			if (prime > limit)
				break;

			if (auto reminder = value % prime)
				++i;
			else
			{
				value /= prime;
				if (prime > lastPrime)
				{
					lastPrime = prime;
					factors.push_back(prime);
				}

				if (value == 1)
					return;
			}
		}

		// Если мы вышли из цикла, то value > 1 и это последний
		// простой множитель, поэтому также добавим его в список
		factors.push_back(value);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
std::vector<unsigned> Solutions::GetPrimes(unsigned limit)
{
	static const uint8_t mask[16] = { 4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0 };

	std::vector<unsigned> primes;

	if (limit > 1)
	{
		primes.reserve(1000);
		primes.push_back(2);

		if (limit > 2)
		{
			const size_t bufferSize = (limit + 13ull) >> 4;
			uint8_t* bits = new uint8_t[bufferSize];
			memset(bits, 0, bufferSize);

			const unsigned lastBit = (limit - 3) >> 1;
			const unsigned k = (limit < 25) ? 0 : (static_cast<unsigned>(sqrt(limit)) - 3) >> 1;

			for (unsigned i = 0; i <= k; ++i)
			{
				if (!(bits[i >> 3] & (1 << (i & 7))))
				{
					unsigned n = (i << 1) + 3;
					primes.push_back(n);

					for (unsigned j = (i + 1) * 3; j <= lastBit; j += n)
						bits[j >> 3] |= 1 << (j & 7);
				}
			}

			auto total = primes.size();
			for (size_t i = (k + 1) >> 3; i < bufferSize; ++i)
			{
				auto v = bits[i];
				total += mask[v & 15] + mask[v >> 4];
			}

			primes.reserve(total);
			for (unsigned i = k + 1; i <= lastBit; ++i)
			{
				if (!(bits[i >> 3] & (1 << (i & 7))))
					primes.push_back((i << 1) + 3);
			}

			delete[] bits;
		}
	}

	return primes;
}
