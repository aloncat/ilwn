//⬪MDPN⬪
#pragma once

#include <core/exception.h>
#include <core/platform.h>

#include <type_traits>

//----------------------------------------------------------------------------------------------------------------------
class EDBLogic : public util::ELogic
{
	AML_EXCEPTION(EDBLogic, util::ELogic)
};

//----------------------------------------------------------------------------------------------------------------------
template<class Excep = util::EAssertion>
class AssertHelper
{
	static_assert(std::is_base_of<util::ELogic, Excep>::value, "Unsupported type");

public:
	template<class T, class = std::enable_if_t<std::is_convertible<T, bool>::value, T>>
	static void Assert(const T& condition, const char* pMsg = nullptr)
	{
		if (!condition)
			Failed(pMsg);
	}

	template<class T, class = std::enable_if_t<std::is_convertible<T, bool>::value, T>>
	static bool Verify(const T& condition, const char* pMsg = nullptr)
	{
		if (!condition)
		{
			Failed(pMsg);
		}
		return true;
	}

protected:
	[[noreturn]] static AML_NOINLINE void Failed(const char* pMsg)
	{
		throw Excep(pMsg ? pMsg : "Assertion failed");
	}
};
