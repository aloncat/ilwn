//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "multisearch.h"
#include "searchbase.h"
#include "util.h"

#include <auxlib/print.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
static int Main(int argCount, const wchar_t* args[])
{
	aux::Printc(L"Equal Sums of Like Powers (ESLP)/iLWN. Standalone app. Ver. 0.30a\n"
		"#8For more information, please visit us at #3https://dmaslov.me/eslp/\n");

	// Программа принимает 3 обязательных и несколько опциональных аргументов командной строки: 1) степень
	// уравнения; 2) количество слагаемых в его левой части; 3) количество слагаемых в его правой части;
	// 4) необязательные значения старших коэффициентов в левой части, с которых будет начат поиск (при их
	// отсутствии будет использовано значение 2 для самого старшего коэффициента); 5) необязательные опции

	if (argCount <= 1)
	{
		// Справка для пустой командной строки
		aux::Printc("#15Usage: #10eslp.exe#7 <power> <left> <right> [factors] [options]\n");
		SearchBase::PrintOptionsHelp();
		return 0;
	}

	if (argCount >= 4)
	{
		int power = wcstol(args[1], nullptr, 10);
		int leftCount = wcstol(args[2], nullptr, 10);
		int rightCount = wcstol(args[3], nullptr, 10);

		constexpr auto maxCount = FactorSearch::MAX_FACTOR_COUNT;
		if (power >= 1 && power <= FactorSearch::MAX_POWER && leftCount >= 1 && rightCount >= 2 &&
			leftCount < maxCount && rightCount < maxCount && leftCount <= rightCount &&
			leftCount + rightCount <= maxCount)
		{
			// Создаём подходящий объект (универсального класса MultiSearch
			// или класса со спецализированным алгоритмом, если такой имеется)
			auto instance = MultiSearch::CreateInstance(power, leftCount, rightCount);

			// Ищем решения. Передаём в функцию только значение степени и количество коэффициентов
			// уравнения (начальные значения старших коэффициентов функция получит самостоятельно).
			// При возникновении ошибки функция вернёт false и выведет сообщение в окно консоли
			return instance->Run(power, leftCount, rightCount) ? 0 : 1;
		}
	}

	// Аргументы в командной строке некорректны
	aux::Printc("#12Error: invalid command line\n");
	return 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();
	InvokeSafe(std::bind(Main, argCount, args));
}
