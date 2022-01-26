//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "diophantine.h"

#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();

	return lab::DiophantineMain(argCount, args);
}
