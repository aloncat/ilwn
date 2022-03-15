//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "search-x23.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchE23 - улучшенный алгоритм для уравнений p.2.3 (для чётных p)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchE23 : public SearchX23
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;
	virtual bool MightHaveSolution(const Task& task) const override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
