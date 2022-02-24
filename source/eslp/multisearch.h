﻿//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "factorsearch.h"

//--------------------------------------------------------------------------------------------------------------------------------
class MultiSearch : public FactorSearch
{
protected:
	virtual bool MightHaveSolution(const Task& task) const override;

	virtual void PerformTask(Worker* worker) override;

	template<class NumberT>
	void SearchFactors(Worker* worker, const NumberT* powers);
};
