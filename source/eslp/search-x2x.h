//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

#include <string>

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

	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};

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

	virtual bool MightHaveSolution(const Task& task) const override;
	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};

//--------------------------------------------------------------------------------------------------------------------------------
class SearchE23 : public SearchX23
{
protected:
	virtual std::wstring GetAdditionalInfo() const override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
