//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "searchfactory.h"

#include "options.h"
#include "search-413.h"
#include "search-414.h"
#include "search-any.h"
#include "search-e1x.h"
#include "search-e23.h"
#include "search-x12.h"
#include "search-x13.h"
#include "search-x22.h"
#include "search-x22i.h"
#include "search-x23.h"

//--------------------------------------------------------------------------------------------------------------------------------
SearchFactory::SearchFactory(int power, int leftCount, int rightCount, const Options& options)
	: m_Power(power)
	, m_LeftCount(leftCount)
	, m_RightCount(rightCount)
	, m_Options(options)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchFactory::RegisterAll()
{
	// Специализированные алгоритмы
	Register<Search413>(INT_MAX);
	Register<Search414>(INT_MAX);

	// Другие кастомные алгоритмы
	Register<SearchE1X>(1);
	Register<SearchE23>(500);
	Register<SearchX12>(100);
	Register<SearchX13>(100);
	Register<SearchX22>(100);
	Register<SearchX23>(100);

	// Экспериментальные алгоритмы
	Register<SearchX22i>(1000);
}

//--------------------------------------------------------------------------------------------------------------------------------
SearchFactory::Instance SearchFactory::CreateInstance(int power, int leftCount, int rightCount, const Options& options)
{
	if (!options.HasOption("nocustom"))
	{
		SearchFactory instance(power, leftCount, rightCount, options);

		instance.RegisterAll();
		if (!instance.m_Items.empty())
		{
			// Сортируем алгоритмы по убыванию приоритета
			std::sort(instance.m_Items.begin(), instance.m_Items.end(), [](const Item& lhs, const Item& rhs) {
				return lhs.priority > rhs.priority;
			});

			return instance.m_Items[0].creator();
		}
	}

	return std::make_unique<SearchAny>();
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
void SearchFactory::Register(int priority)
{
	class Helper : public T {
	public:
		using T::IsSuitable;
	};

	Add([]() -> Instance { return std::make_unique<T>(); },
		&Helper::IsSuitable, priority);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchFactory::Add(CreateFn creator, SuitableFn isSuitable, int priority)
{
	if (priority > 0)
	{
		bool allowAll = m_Options.HasOption("allowall");
		if (isSuitable(m_Power, m_LeftCount, m_RightCount, allowAll))
			m_Items.push_back({ priority, creator });
	}
}
