//∙MDPN
#include "pch.h"
#include "version.h"

#include <core/auxutil.h>
#include <core/platform.h>
#include <core/util.h>

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::string GetAppVersion()
{
	std::string s(util::IsDebugBuild() ? "#12[DBG]#8 " : "");
	s.append(aux::GetBuildDateTime(__DATE__, __TIME__));
	return s;
}
