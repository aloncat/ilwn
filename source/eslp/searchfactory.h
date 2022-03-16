//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "options.h"
#include "searchbase.h"

#include <core/util.h>

#include <memory>
#include <vector>

//--------------------------------------------------------------------------------------------------------------------------------
class SearchFactory final
{
	AML_NONCOPYABLE(SearchFactory)

public:
	using Instance = std::unique_ptr<SearchBase>;

	// Создаёт экземпляр наиболее подходящего (оптимального) класса поиска
	static Instance CreateInstance(int power, int leftCount, int rightCount, const Options& options);

private:
	using CreateFn = Instance (*)();
	using SuitableFn = bool (*)(int power, int leftCount, int rightCount, bool allowAll);

	SearchFactory(int power, int leftCount, int rightCount, const Options& options);

	void RegisterAll();
	template<class T> void Register(int priority = 1);
	void Add(CreateFn creator, SuitableFn priorityFn, int priority);

private:
	struct Item {
		int priority;
		CreateFn creator;
	};

	int m_Power;
	int m_LeftCount;
	int m_RightCount;
	const Options& m_Options;

	std::vector<Item> m_Items;
};
