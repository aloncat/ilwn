//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"
#include "hashtable.h"

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX22 - улучшенный алгоритм для уравнений x.2.2
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX22 : public FactorSearch
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void BeforeCompute(unsigned upperLimit) override;
	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

protected:
	HashTable<23> m_Hashes;
	bool m_IsEvenPower = false;
};
