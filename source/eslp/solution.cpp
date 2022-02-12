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
	Assert(leftCount > 0);
	for (int i = 0; i < leftCount; ++i)
		left.push_back(allFactors[i]);

	Assert(rightCount > 0);
	allFactors += leftCount;
	for (int i = 0; i < rightCount; ++i)
		right.push_back(allFactors[i]);
}

//--------------------------------------------------------------------------------------------------------------------------------
Solution::Solution(const unsigned* leftFactors, const unsigned* rightFactors, int leftCount, int rightCount)
{
	Assert(leftCount > 0);
	for (int i = 0; i < leftCount; ++i)
		left.push_back(leftFactors[i]);

	Assert(rightCount > 0);
	for (int i = 0; i < rightCount; ++i)
		right.push_back(rightFactors[i]);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Solution::SortFactors()
{
	if (left.size() > 1)
	{
		std::sort(left.begin(), left.end(), [](unsigned lhs, unsigned rhs) {
			return lhs > rhs;
		});
	}

	if (right.size() > 1)
	{
		std::sort(right.begin(), right.end(), [](unsigned lhs, unsigned rhs) {
			return lhs > rhs;
		});
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::operator ==(const Solution& rhs) const
{
	if (size_t count = left.size(); Verify(count && count == rhs.left.size()))
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (left[i] != rhs.left[i])
				return false;
		}

		if (count = right.size(); Verify(count && count == rhs.right.size()))
		{
			for (size_t i = 0; i < count; ++i)
			{
				if (right[i] != rhs.right[i])
					return false;
			}

			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::operator <(const Solution& rhs) const
{
	if (size_t count = left.size(); Verify(count && count == rhs.left.size()))
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (left[i] != rhs.left[i])
			{
				return left[i] < rhs.left[i];
			}
		}

		if (count = right.size(); Verify(count && count == rhs.right.size()))
		{
			for (size_t i = 0; i < count; ++i)
			{
				if (right[i] != rhs.right[i])
				{
					return right[i] < rhs.right[i];
				}
			}
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
bool Solutions::Insert(const Solution& s)
{
	if (!IsPrimitive(s))
		return false;

	Solution sorted = s;
	sorted.SortFactors();

	auto result = m_Solutions.insert(std::move(sorted));
	return result.second;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solutions::IsPrimitive(const Solution& s)
{
	const size_t leftCount = s.left.size();
	const size_t rightCount = s.right.size();

	if (!Verify(leftCount && rightCount))
		return false;

	const size_t total = leftCount + rightCount;
	util::SmartArray<unsigned, 500> allFactors(total);
	unsigned lowest = UINT_MAX;

	size_t k = 0;
	for (auto f : s.left)
	{
		allFactors[k++] = f;
		if (f < lowest)
			lowest = f;
	}
	for (auto f : s.right)
	{
		allFactors[k++] = f;
		if (f < lowest)
			lowest = f;
	}

	if (m_Primes.empty() || m_Primes.back() < lowest)
	{
		constexpr unsigned add = 10000;
		m_Primes = std::vector<unsigned>();
		m_Primes = GetPrimes(std::min(lowest, UINT_MAX - add) + add);
	}

	for (unsigned p : m_Primes)
	{
		if (p > lowest)
			break;

		bool allDivisible = true;
		for (size_t i = 0; i < total; ++i)
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
std::vector<unsigned> Solutions::GetPrimes(unsigned limit)
{
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
			for (unsigned i = k + 1; i <= lastBit; ++i)
			{
				if (!(bits[i >> 3] & (1 << (i & 7))))
					++total;
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
