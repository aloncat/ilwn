//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX1XCommon : public FactorSearch
{
protected:
	virtual void SelectNextTask(Task& task) override;
	virtual bool MightHaveSolution(const Task& task) const override;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX12 - улучшенный алгоритм для уравнений x.1.2
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX12 : public SearchX1XCommon
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchX13 - улучшенный алгоритм для уравнений x.1.3
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchX13 : public SearchX1XCommon
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
