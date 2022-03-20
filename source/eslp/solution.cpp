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
	left.reserve(leftCount);
	for (int i = 0; i < leftCount; ++i)
		left.push_back(allFactors[i]);

	Assert(rightCount > 0);
	allFactors += leftCount;
	right.reserve(rightCount);
	for (int i = 0; i < rightCount; ++i)
		right.push_back(allFactors[i]);
}

//--------------------------------------------------------------------------------------------------------------------------------
Solution::Solution(const unsigned* leftFactors, const unsigned* rightFactors, int leftCount, int rightCount)
{
	Assert(leftCount > 0);
	left.reserve(leftCount);
	for (int i = 0; i < leftCount; ++i)
		left.push_back(leftFactors[i]);

	Assert(rightCount > 0);
	right.reserve(rightCount);
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
void Solution::Swap(Solution& that)
{
	if (this != &that)
	{
		left.swap(that.left);
		right.swap(that.right);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::IsConfluent() const
{
	for (unsigned l : left)
	{
		for (unsigned r : right)
		{
			if (l == r)
				return true;
		}
	}

	return false;
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

//--------------------------------------------------------------------------------------------------------------------------------
bool Solution::IsLower(const unsigned* factors, int count) const noexcept
{
	if (const int leftCount = static_cast<int>(left.size()); count <= leftCount)
	{
		for (int i = 0; i < count; ++i)
		{
			if (auto f = left[i]; factors[i] != f)
				return f < factors[i];
		}
	} else
	{
		for (int i = 0; i < leftCount; ++i)
		{
			if (auto f = left[i]; factors[i] != f)
				return f < factors[i];
		}

		for (int i = leftCount; i < count; ++i)
		{
			if (auto f = right[i - leftCount]; factors[i] != f)
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
	const size_t leftCount = solution.left.size();
	const size_t rightCount = solution.right.size();

	if (!Verify(leftCount && rightCount))
		return false;

	const size_t total = leftCount + rightCount;
	util::SmartArray<unsigned, 500> allFactors(total);
	unsigned lowest = UINT_MAX;

	size_t k = 0;
	for (auto f : solution.left)
	{
		allFactors[k++] = f;
		lowest = (f < lowest) ? f : lowest;
	}
	for (auto f : solution.right)
	{
		allFactors[k++] = f;
		lowest = (f < lowest) ? f : lowest;
	}

	m_Factors.clear();
	GetFactors(m_Factors, lowest);

	for (unsigned p : m_Factors)
	{
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
