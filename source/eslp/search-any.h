//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchAny - универсальный алгоритм для уравнений p.m.n
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchAny : public FactorSearch
{
protected:
	virtual void BeforeCompute(unsigned upperLimit) override;
	CheckTaskFn GetCheckTaskFn() const;

	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	template<class NumberT>
	bool SkipLowSet(Task& task, const NumberT* powers) const;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

protected:
	using SkipLowSetFn = bool (SearchAny::*)(Task& task, const void* powers) const;

	SkipLowSetFn m_SkipFn = nullptr;
};
