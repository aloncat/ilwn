//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Search413 - специализированный алгоритм для уравнения 4.1.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Search413 : public FactorSearch
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void SelectNextTask(Task& task) override;
	virtual unsigned GetChunkSize(unsigned hiFactor) override;
	virtual bool MightHaveSolution(const Task& task) const override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
