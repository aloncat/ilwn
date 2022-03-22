//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "search-x23.h"

#include <string>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchE23 - улучшенный алгоритм для уравнений k.2.3 (для чётных k)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchE23 : public SearchX23
{
protected:
	static bool IsSuitable(int power, int leftCount, int rightCount, bool allowAll);
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
