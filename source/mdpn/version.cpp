//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "version.h"

#include <core/platform.h>
#include <core/strutil.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::string GetAppVersion()
{
	return util::IsDebugBuild() ? std::string("#12[DEBUG]") :
		GetBuildDateTime(__DATE__, __TIME__);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string GetBuildDateTime(const char* date, const char* time)
{
	int day = 1, month = 0, year = 2000;
	if (date && strlen(date) > 4)
	{
		sscanf_s(date + 4, "%d %d", &day, &year);
		const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
		for (; month < 11 && util::StrNInsCmp(date, &months[3 * month], 3); ++month);
	}

	char dateTime[28];
	// 28 байт - минимальный размер, достаточный на случай некорректных входных данных
	sprintf_s(dateTime, util::CountOf(dateTime), "%4d-%02d-%02d ", year, month + 1, day);

	if (time && *time)
	{
		strncpy_s(dateTime + 11, util::CountOf(dateTime) - 11, time, 5);
	}

	return std::string(dateTime, (time && *time) ? 16 : 10);
}
