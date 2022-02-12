//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "util.h"

#include <core/array.h>
#include <core/strformat.h>

//--------------------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE std::string SeparateWithCommas(const char* str, size_t size, char separator)
{
	const size_t outSize = (size > 3) ? size + (size - 1) / 3 : size;
	util::SmartArray<char, 256> buffer(outSize);

	char* out = buffer;
	for (size_t p = 0, l = size % 3; p < size;)
	{
		if (p && separator)
			*out++ = separator;
		for (size_t i = (p || !l) ? 3 : l; i; --i)
			*out++ = str[p++];
	}

	return std::string(buffer, outSize);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string SeparateWithCommas(uint64_t number, char separator)
{
	util::Formatter<char, 24> fmt;
	fmt << number;

	return SeparateWithCommas(fmt.GetData(), fmt.GetSize(), separator);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string SeparateWithCommas(std::string_view str, char separator)
{
	return SeparateWithCommas(str.data(), str.size(), separator);
}
