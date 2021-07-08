//∙MDPN
#include "pch.h"
#include "dbprogress.h"

#include <core/auxutil.h>
#include <core/platform.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

//----------------------------------------------------------------------------------------------------------------------
class DBProgress::Shower final
{
	AML_NONCOPYABLE(Shower)

public:
	Shower(const char* pFmt);
	~Shower();

	bool IsReady();
	void Show(float progress);

private:
	std::string m_Fmt;
	size_t m_MaxLen = 0;
	uint32_t m_LastTick = 0;
};

//----------------------------------------------------------------------------------------------------------------------
DBProgress::DBProgress(const Callback& cb)
	: m_Cb(cb)
{
}

//----------------------------------------------------------------------------------------------------------------------
DBProgress::DBProgress(const std::string& fmt)
	: DBProgress(fmt.c_str())
{
}

//----------------------------------------------------------------------------------------------------------------------
DBProgress::DBProgress(const char* pFmt)
{
	if (pFmt && *pFmt)
	{
		auto shower = std::make_shared<Shower>(pFmt);
		m_Cb = [shower = std::move(shower)](float progress) {
			if (shower && shower->IsReady())
				shower->Show(progress);
		};
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DBProgress::operator ()(float progress)
{
	if (m_Cb)
	{
		progress = std::min(progress, 100.f);
		m_LastValue = std::max(m_LastValue, progress);
		m_Cb(m_LastValue);
	}
}

//----------------------------------------------------------------------------------------------------------------------
DBProgress::Shower::Shower(const char* pFmt)
	: m_Fmt(pFmt ? pFmt : "%.0f")
{
}

//----------------------------------------------------------------------------------------------------------------------
DBProgress::Shower::~Shower()
{
	if (m_MaxLen)
	{
		std::string s(m_MaxLen, ' ');
		aux::Print("\r" + s + "\r");
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool DBProgress::Shower::IsReady()
{
	const uint32_t tick = ::GetTickCount() | 1;
	if (m_LastTick && tick - m_LastTick < 250)
		return false;

	m_LastTick = tick;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void DBProgress::Shower::Show(float progress)
{
	auto msg = util::Format(m_Fmt.c_str(), progress);
	if (!msg.empty())
	{
		m_MaxLen = std::max(m_MaxLen, msg.size());
		aux::Printc("\r" + msg);
	}
}
