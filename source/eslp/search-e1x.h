//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchE1X - улучшенный алгоритм для уравнений p.1.n (для чётных p при некоторых n)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchE1X : public FactorSearch
{
public:
	static bool IsSuitable(int power, int leftCount, int rightCount);

protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual bool MightHaveSolution(const Task& task) const override;
	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
