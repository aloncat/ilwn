//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX23 - улучшенный алгоритм для уравнений x.2.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX23 : public FactorSearch
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
