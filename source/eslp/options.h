//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/util.h>

#include <string>
#include <string_view>

#include <set>

//--------------------------------------------------------------------------------------------------------------------------------
struct OptionKey {
	const std::string key;		// Имя опции (в нижнем регистре)
	const std::wstring value;	// Строковое значение (если есть)

public:
	explicit OptionKey(std::string_view _key, std::wstring_view _value = L"");

	// Возвращает true, если значение value - не пустая строка
	bool HasValue() const;

	// Возвращает true, если value содержит число
	bool HasNumericValue() const;

	// Возвращает значение value, если оно содержит число. Если
	// value не является корректным числом, то функция вернёт 0
	int GetNumericValue() const;

	bool operator <(std::string_view rhs) const { return key < rhs; }
	bool operator <(const OptionKey& rhs) const { return key < rhs.key; }
	friend bool operator <(std::string_view lhs, const OptionKey& rhs) { return lhs < rhs.key; }
};

//--------------------------------------------------------------------------------------------------------------------------------
class Options final
{
	AML_NONCOPYABLE(Options)

public:
	Options();

	// Добавляет опцию в набор. Если такая опция уже есть,
	// то её существующее значение будет заменено на value
	void AddOption(std::string_view key, std::wstring_view value = L"");
	void AddOption(std::wstring_view key, std::wstring_view value = L"");

	// Возвращает true, если указанная опция существует
	bool HasOption(std::string_view key) const;

	// Возвращает опцию, соответствующую ключу key
	const OptionKey& operator[](std::string_view key) const;

private:
	static std::string SanitizeKey(std::string_view key);

	std::set<OptionKey, std::less<>> m_Options;
};
