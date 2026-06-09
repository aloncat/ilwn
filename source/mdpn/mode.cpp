//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "mode.h"

#include <auxlib/print.h>
#include <core/platform.h>
#include <core/strutil.h>

//--------------------------------------------------------------------------------------------------------------------------------
bool Mode::Run()
{
	OnCmdNotRecognized();
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Mode::OnCmdNotRecognized() const
{
	aux::Printc("#12Error: #7command not recognized. Run with '#14help#7' to get details on usage\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
void Mode::OnInvalidCmdOptions() const
{
	aux::Printc("#12Error: #7otions not recognized. Run with '#14help [command]#7' to get details on usage\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
void Mode::OnInvalidCmdLine() const
{
	aux::Printc("#12Error: #7invalid command line. Run with '#14help#7' to get details on usage\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
Mode::Helper::Helper(int argCount, const wchar_t* args[])
{
	m_Mode = std::make_unique<Mode>();

	if (argCount > 1 && args)
	{
		auto& params = m_Mode->m_Params;
		params.reserve(argCount - 1);

		for (int i = 1; i < argCount; ++i)
		{
			const wchar_t* p = args[i];
			params.push_back(util::ToAnsi(p ? p : L""));
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Mode::Helper::IsCommand(std::string_view cmd, bool optional) const
{
	if (cmd.empty())
		return false;

	if (m_Mode->m_Params.empty())
		return optional;

	// TODO: Нужны полноценные версии функций util::Str*Cmp(), как в новом AML
	return !util::StrInsCmp(m_Mode->m_Params[0], cmd.data());
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Mode::Helper::Run()
{
	return m_Mode ? m_Mode->Run() : false;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Mode::Helper::SetNewMode(std::unique_ptr<Mode>&& newMode)
{
	std::swap(m_Mode->m_Params, newMode->m_Params);
	m_Mode = std::move(newMode);
}
