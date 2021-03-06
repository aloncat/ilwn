//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Search415 - специализированный алгоритм для уравнения 4.1.5
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Search415 : public FactorSearch
{
protected:
	static bool IsSuitable(int power, int leftCount, int rightCount, bool allowAll);
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;
	virtual unsigned GetChunkSize(unsigned hiFactor) override;

	virtual void InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(WorkerTask& task) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	template<class NumberT>
	void Search5(Worker* worker, unsigned* k, const NumberT* powers);

	template<class NumberT>
	void SearchOdd(Worker* worker, unsigned* k, const NumberT* powers);

	template<class NumberT>
	bool SearchLast2(Worker* worker, unsigned* k, NumberT z, const NumberT* powers);

	template<class NumberT>
	bool SearchLast10(Worker* worker, unsigned* k, NumberT z, unsigned limit, const NumberT* powers);

protected:
	unsigned m_ProgressMask = 0;
};
