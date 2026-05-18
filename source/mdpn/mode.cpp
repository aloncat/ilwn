//∙MDPN
#include "pch.h"
#include "mode.h"

#include <core/auxutil.h>
#include <core/strutil.h>

//----------------------------------------------------------------------------------------------------------------------
std::unique_ptr<Mode> Mode::Create(int argC, const wchar_t* argA[])
{
	auto obj = std::make_unique<Mode>();
	obj->SetParams(argC, argA);
	return obj;
}

//----------------------------------------------------------------------------------------------------------------------
bool Mode::IsCommand(const char* pCmd, bool optional) const
{
	if (!pCmd || !pCmd[0])
		return false;
	if (m_Params.empty())
		return optional;

	return !util::StrInsCmp(m_Params[0], pCmd);
}

//----------------------------------------------------------------------------------------------------------------------
bool Mode::Run()
{
	OnCmdNotRecognized();
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void Mode::SetParams(int argC, const wchar_t* argA[])
{
	m_Params.clear();
	if (argC > 1 && argA)
	{
		m_Params.reserve(argC - 1);
		for (int i = 1; i < argC; ++i)
		{
			const wchar_t* p = argA[i];
			m_Params.push_back(util::ToAnsi(p ? p : L""));
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Mode::OnCmdNotRecognized() const
{
	aux::Printc("#12Error: #7command not recognized. Use #14--help#7 to get details on usage\n");
}

//----------------------------------------------------------------------------------------------------------------------
void Mode::OnInvalidCmdLine() const
{
	aux::Printc("#12Error: #7invalid command line. Use #14--help#7 to get details on usage\n");
}
