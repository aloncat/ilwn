//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "util.h"

#include <auxlib/print.h>
#include <core/array.h>
#include <core/exception.h>
#include <core/strformat.h>
#include <core/winapi.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные функции
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
bool IsInteger(std::string_view str)
{
	if (str.empty())
		return false;

	if (str[0] == '-')
		str.remove_prefix(1);

	return IsPositiveInteger(str);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool IsInteger(std::wstring_view str)
{
	if (str.empty())
		return false;

	if (str[0] == '-')
		str.remove_prefix(1);

	return IsPositiveInteger(str);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool IsPositiveInteger(std::string_view str)
{
	const char* p = str.data();
	const size_t count = str.size();

	if (!count)
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		if (p[i] < '0' || p[i] > '9')
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool IsPositiveInteger(std::wstring_view str)
{
	const wchar_t* p = str.data();
	const size_t count = str.size();

	if (!count)
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		if (p[i] < '0' || p[i] > '9')
			return false;
	}

	return true;
}

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция InvokeSafe
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
static int CppExceptionGuard(const std::function<int()>& fn, int errorCode)
{
	try
	{
		return fn ? fn() : 0;
	}
	catch (const util::EGeneric& e)
	{
		aux::Printf("\r#12Fatal error: unhandled exception [%s] -> %s \n",
			e.ClassName(), e.what());
	}
	catch (const std::exception& e)
	{
		aux::Printf("\r#12Fatal error: unhandled c++ exception -> %s \n", e.what());
	}
	catch (...)
	{
		aux::Printc("\r#12Fatal error: unhandled c++ exception \n");
	}

	return errorCode;
}

//--------------------------------------------------------------------------------------------------------------------------------
int InvokeSafe(const std::function<int()>& fn, int errorCode)
{
	__try
	{
		return CppExceptionGuard(fn, errorCode);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		aux::Printf("\r#12Fatal error: unhandled exception %08X \n",
			::GetExceptionCode());
	}

	return errorCode;
}
