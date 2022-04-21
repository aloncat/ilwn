//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchAny - универсальный алгоритм для уравнений k.m.n
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchAny : public FactorSearch
{
protected:
	using SelectNextFn = void (SearchAny::*)(WorkerTask& task, const void* powers) const;

	virtual void BeforeCompute(unsigned upperLimit) override;
	CheckTaskFn GetCheckTaskFn() const;

	virtual void InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(WorkerTask& task) override;

	template<class NumberT>
	SelectNextFn GetSelectNextFn();

	template<class NumberT>
	void SelectNext(WorkerTask& task, const NumberT* powers) const;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	template<class NumberT>
	bool SearchLast(Worker* worker, NumberT z, unsigned* k, const NumberT* powers);

	template<class NumberT>
	SearchFn GetSearchFn(int freeFactors);

	template<class NumberT>
	void SearchFactors2(Worker* worker, const NumberT* powers);

	template<class NumberT>
	void SearchFactors3(Worker* worker, const NumberT* powers);

protected:
	SelectNextFn m_SelectNextFn = nullptr;
	int m_FactorCount = 0;
};
