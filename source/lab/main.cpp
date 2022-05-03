//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include <core/util.h>

namespace lab {

// Главные функции экспериментов
int DiophantineMain(int, const wchar_t* []);

}

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();

	using namespace lab;
	return DiophantineMain(argCount, args);
}
