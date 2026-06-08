//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/exception.h>
#include <core/platform.h>

//--------------------------------------------------------------------------------------------------------------------------------
class EDBLogic : public util::ELogic
{
	AML_EXCEPTION(EDBLogic, util::ELogic)
};

//--------------------------------------------------------------------------------------------------------------------------------
template<class Excep = util::EAssertion>
class AssertHelper
{
	static_assert(std::is_base_of_v<util::ELogic, Excep>, "Unsupported type");

public:
	template<class T, class = std::enable_if_t<std::is_convertible_v<T, bool>, T>>
	static void Assert(const T& condition, const char* msg = nullptr)
	{
		if (!condition)
			Failed(msg);
	}

	template<class T, class = std::enable_if_t<std::is_convertible_v<T, bool>, T>>
	static bool Verify(const T& condition, const char* msg = nullptr)
	{
		if (!condition)
		{
			Failed(msg);
		}
		return true;
	}

protected:
	[[noreturn]] static AML_NOINLINE void Failed(const char* msg)
	{
		throw Excep(msg ? msg : "Assertion failed");
	}
};
