﻿//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "options.h"

#include "util.h"

#include <core/debug.h>
#include <core/strutil.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   OptionKey
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
OptionKey::OptionKey(std::wstring_view _key, std::wstring_view _value)
	: key(_key)
	, value(_value)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
bool OptionKey::HasValue() const
{
	return !util::Trim(value).empty();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool OptionKey::HasNumericValue() const
{
	const auto s = util::Trim(value);
	return !s.empty() && IsInteger(s);
}

//--------------------------------------------------------------------------------------------------------------------------------
int OptionKey::GetNumericValue() const
{
	if (auto s = util::Trim(value); !s.empty() && IsInteger(s))
		return wcstol(s.c_str(), nullptr, 10);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Options
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
Options::Options()
{
	m_Options.emplace(L"");
}

//--------------------------------------------------------------------------------------------------------------------------------
void Options::AddOption(std::wstring_view key, std::wstring_view value)
{
	if (auto s = SanitizeKey(key); Verify(!s.empty()))
	{
		if (auto result = m_Options.emplace(s, value); !result.second)
		{
			auto& v = result.first->value;
			const_cast<std::wstring&>(v) = value;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Options::HasOption(std::wstring_view key) const
{
	if (auto s = SanitizeKey(key); !s.empty())
		return m_Options.find(s) != m_Options.end();

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
const OptionKey& Options::operator[](std::wstring_view key) const
{
	if (auto s = SanitizeKey(key); Verify(!s.empty()))
	{
		if (auto it = m_Options.find(s); it != m_Options.end())
			return *it;
	}

	// NB: если запрошенной опции в наборе нет, то вернём "пустую" опцию. Так
	// как опции без имени недоупстимы, то "пустая" всегда будет самой первой
	return *m_Options.begin();
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Options::SanitizeKey(std::wstring_view key)
{
	std::wstring s = util::Trim(key);
	util::LoCaseInplace(s, true);
	return s;
}
