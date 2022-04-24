//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Search413 - специализированный алгоритм для уравнения 4.1.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Search413 : public FactorSearch
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

protected:
	unsigned m_ProgressMask = 0;
};
