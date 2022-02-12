//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/util.h>

#include <set>
#include <vector>

//--------------------------------------------------------------------------------------------------------------------------------
struct Solution
{
	// Коэффициенты левой части
	std::vector<unsigned> left;
	// Коэффициенты правой части
	std::vector<unsigned> right;

public:
	Solution(const unsigned* allFactors, int leftCount, int rightCount);
	Solution(const unsigned* leftFactors, const unsigned* rightFactors, int leftCount, int rightCount);

	void SortFactors();

	bool operator ==(const Solution& rhs) const;
	bool operator <(const Solution& rhs) const;
};

//--------------------------------------------------------------------------------------------------------------------------------
class Solutions
{
	AML_NONCOPYABLE(Solutions)

public:
	Solutions() = default;

	// Добавляет решение в контейнер. Если решение не примитивное, или если такое
	// решение уже существует, то оно добавлено не будет, и функция вернёт false
	bool Insert(const Solution& s);

	void Clear() { m_Solutions.clear(); }

	size_t Count() const { return m_Solutions.size(); }

protected:
	bool IsPrimitive(const Solution& s);
	static std::vector<unsigned> GetPrimes(unsigned limit);

	std::set<Solution> m_Solutions;		// Все найденные решения уравнения
	std::vector<unsigned> m_Primes;		// Набор простых чисел (начиная с 2)
};
