//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchE1X - улучшенный алгоритм для уравнений k.1.n (для чётных k при некоторых n)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchE1X : public FactorSearch
{
protected:
	static bool IsSuitable(int power, int leftCount, int rightCount, bool allowAll);
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;
	CheckTaskFn GetCheckTaskFn() const;

	virtual void SelectNextTask(WorkerTask& task) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
