﻿//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"
#include "hashtable.h"

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

	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);

	template<class NumberT>
	bool SearchLast(Worker* worker, NumberT z, unsigned* k, const NumberT* powers);

protected:
	HashTable<20> m_Hashes;
	unsigned m_ProgressMask = 0;
};
