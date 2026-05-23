//⬪AML⬪
#include "pch.h"
#include "threadsync.h"

#include "winapi.h"

using namespace thread;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CriticalSection
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//----------------------------------------------------------------------------------------------------------------------
CriticalSection::CriticalSection(unsigned spinCount)
{
	CRITICAL_SECTION* pSection = (sizeof(CRITICAL_SECTION) <= sizeof(m_DataA)) ?
		reinterpret_cast<CRITICAL_SECTION*>(m_DataA) : new CRITICAL_SECTION;
	::InitializeCriticalSectionAndSpinCount(pSection, spinCount);
	m_pData = pSection;
}

//----------------------------------------------------------------------------------------------------------------------
CriticalSection::~CriticalSection()
{
	CRITICAL_SECTION* pSection = static_cast<CRITICAL_SECTION*>(m_pData);
	::DeleteCriticalSection(pSection);
	if (m_pData != m_DataA)
		delete pSection;
}

//----------------------------------------------------------------------------------------------------------------------
bool CriticalSection::TryEnter()
{
	CRITICAL_SECTION* pSection = static_cast<CRITICAL_SECTION*>(m_pData);
	return ::TryEnterCriticalSection(pSection) != 0;
}

//----------------------------------------------------------------------------------------------------------------------
void CriticalSection::Enter()
{
	CRITICAL_SECTION* pSection = static_cast<CRITICAL_SECTION*>(m_pData);
	::EnterCriticalSection(pSection);
}

//----------------------------------------------------------------------------------------------------------------------
void CriticalSection::Leave()
{
	CRITICAL_SECTION* pSection = static_cast<CRITICAL_SECTION*>(m_pData);
	::LeaveCriticalSection(pSection);
}

#else
	#error Not implemented
#endif // AML_OS_WINDOWS
