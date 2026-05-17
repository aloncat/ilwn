//∙MDPN
#include "pch.h"
#include "version.h"

#include <core/auxutil.h>
#include <core/platform.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::string GetAppVersion()
{
	return util::IsDebugBuild() ? std::string("#12[DEBUG]") :
		aux::GetBuildDateTime(__DATE__, __TIME__);
}
