//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "multisearch.h"

#include "search-413.h"
#include "search-414.h"
#include "search-any.h"
#include "search-e1x.h"
#include "search-e23.h"
#include "search-x12.h"
#include "search-x13.h"
#include "search-x22.h"
#include "search-x23.h"

//--------------------------------------------------------------------------------------------------------------------------------
MultiSearch::Instance MultiSearch::CreateInstance(int power, int leftCount, int rightCount, const Options& options)
{
	if (!options.HasOption("nocustom"))
	{
		if (leftCount == 1)
		{
			if (power == 4)
			{
				if (rightCount == 3)
					return std::make_unique<Search413>();
				if (rightCount == 4)
					return std::make_unique<Search414>();
			}

			if (rightCount == 2)
				return std::make_unique<SearchX12>();
			if (rightCount == 3)
				return std::make_unique<SearchX13>();
			if (SearchE1X::IsSuitable(power, leftCount, rightCount))
				return std::make_unique<SearchE1X>();
		}
		else if (leftCount == 2)
		{
			if (rightCount == 2)
				return std::make_unique<SearchX22>();

			if (rightCount == 3 && power <= 20)
			{
				// X23 и E23 - оптимизированные алгоритмы для уравнения вида x.2.3. X23 подходит для любых степеней;
				// но E23 имеет дополнительные оптимизации и подходит только для чётных степеней до 20-й включительно
				return (power & 1) ? std::make_unique<SearchX23>() :
					std::make_unique<SearchE23>();
			}
		}
	}

	// Дефолтный универсальный алгоритм
	return std::make_unique<SearchAny>();
}
