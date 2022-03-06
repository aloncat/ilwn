﻿//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"
#include "options.h"

#include <memory>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   MultiSearch - универсальный алгоритм поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class MultiSearch : public FactorSearch
{
public:
	using Instance = std::unique_ptr<FactorSearch>;

	// Создаёт экземпляр наиболее подходящего (оптимального) класса поиска
	static Instance CreateInstance(int power, int leftCount, int rightCount, const Options& options);

protected:
	virtual void InitFirstTask(Task& task, const std::vector<unsigned>& startFactors) override;
	virtual void SelectNextTask(Task& task) override;

	template<class NumberT>
	bool SkipLowSet(Task& task, const NumberT* powers) const;

	virtual bool MightHaveSolution(const Task& task) const override;
	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
