//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/util.h>

#include <set>
#include <vector>

//--------------------------------------------------------------------------------------------------------------------------------
class Solution
{
public:
	Solution(const unsigned* allFactors, int leftCount, int rightCount);
	Solution(const unsigned* leftFactors, const unsigned* rightFactors, int leftCount, int rightCount);

	int GetLCount() const { return m_LeftCount; }
	int GetRCount() const { return static_cast<int>(m_Factors.size()) - m_LeftCount; }

	const unsigned* GetLFactors() const { return m_Factors.data(); }
	const unsigned* GetRFactors() const { return m_Factors.data() + m_LeftCount; }

	void SortFactors();

	void Swap(Solution& that);

	// Возвращает true, если решение вырождено, то есть если некоторый коэффициент
	// из левой части совпадает по значению с каким-либо коэффициентом из правой
	bool IsConfluent() const;

	bool operator ==(const Solution& rhs) const;
	bool operator <(const Solution& rhs) const;

	// Возвращает true, если указанные старшие коэффициенты
	// (обычно задания) меньше старших коэффициентов решения
	bool IsLower(const unsigned* factors, int count) const;

protected:
	int m_LeftCount = 0;				// Количество коэф-в в левой части
	std::vector<unsigned> m_Factors;	// Коэффициенты решения
};

//--------------------------------------------------------------------------------------------------------------------------------
class Solutions
{
	AML_NONCOPYABLE(Solutions)

public:
	Solutions() = default;

	// Добавляет решение в контейнер. Если такое решение уже существует, то оно добавлено не будет.
	// Если флаг verify установлен, то решение будет проверено на примитивность (не примитивное
	// решение добавлено не будет). Функция вернёт true, если решение будет добавлено
	bool Insert(const Solution& solution, bool verify = true);
	bool Insert(Solution&& solution, bool verify = true);

	void Clear() { m_Solutions.clear(); }

	// Удаляет все решения, старшие коэффициенты которых меньше указанных коэффициентов задания
	void Prune(const unsigned* factors, int count);

	size_t Count() const { return m_Solutions.size(); }

	// Возвращает true, если указанное решение solution примитивно, то
	// есть если у его коэффициентов нет общего делителя больше единицы
	bool IsPrimitive(const Solution& solution);

protected:
	// Добавляет в factors все уникальные простые множители value
	void GetFactors(std::vector<unsigned>& factors, unsigned value);

	// Генерирует все простые числа до limit включительно
	static std::vector<unsigned> GetPrimes(unsigned limit);

	std::set<Solution> m_Solutions;		// Все найденные решения уравнения
	std::vector<unsigned> m_Primes;		// Набор простых чисел (начиная с 2)
	std::vector<unsigned> m_Factors;	// Множители наименьшего коэффициента
};
