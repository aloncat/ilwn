//⬪AML⬪
#include "pch.h"
#include "exception.h"

#include "platform.h"

#include <stdlib.h>
#include <string.h>

using namespace util;

//----------------------------------------------------------------------------------------------------------------------
EGeneric::EGeneric() noexcept
	: m_pWhat(nullptr)
{
}

//----------------------------------------------------------------------------------------------------------------------
EGeneric::EGeneric(const char* pMsg) noexcept
	: m_pWhat(CopyString(pMsg))
{
}

//----------------------------------------------------------------------------------------------------------------------
EGeneric::EGeneric(const EGeneric& that) noexcept
	: m_pWhat(CopyString(that.m_pWhat))
{
}

//----------------------------------------------------------------------------------------------------------------------
EGeneric::~EGeneric()
{
	Tidy();
}

//----------------------------------------------------------------------------------------------------------------------
const char* EGeneric::what() const noexcept
{
	return m_pWhat ? m_pWhat : "";
}

//----------------------------------------------------------------------------------------------------------------------
EGeneric& EGeneric::operator =(const EGeneric& that) noexcept
{
	if (this != &that)
	{
		Tidy();
		m_pWhat = CopyString(that.m_pWhat);
	}
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
void EGeneric::Tidy()
{
	if (m_pWhat)
	{
		free(const_cast<char*>(m_pWhat));
		m_pWhat = nullptr;
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE const char* EGeneric::CopyString(const char* pStr)
{
	if (!pStr)
		return nullptr;

	const size_t size = strlen(pStr) + 1;
	if (size <= 1)
		return nullptr;

	char* pBuffer = static_cast<char*>(malloc(size));
	if (pBuffer)
		memcpy(pBuffer, pStr, size);
	return pBuffer;
}
