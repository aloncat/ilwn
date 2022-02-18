//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "factorsearch.h"

#include <auxlib/print.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();

	aux::Printc(L"Equal Sums of Like Powers (ESLP)/iLWN. Standalone app. Ver. 0.30a\n"
		"#8For more information, please visit us at #9https://dmaslov.me/eslp/\n");

	// Программа принимает 3 обязательных и 1 опциональный аргумент командной строки: 1) степень
	// уравнения, 2) количество слагаемых в его левой части, 3) количество слагаемых в его правой
	// части, 4) опциональное значение самого старшего коэффициента левой части, с которого будет
	// начат поиск (при его отсутствии будет использовано значение 2)

	if (argCount <= 1)
	{
		// Справка для пустой командной строки
		aux::Printc("#15Usage: #6eslp.exe#7 power left right [factor]\n");
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
			FactorSearch search;
			// Ищем решения. Передаём в функцию только значение степени и количество коэффициентов
			// уравнения (начальное значение старшего коэффициента функция получит самостоятельно).
			// При возникновении ошибки функция вернёт false и выведет сообщение в окно консоли
			return search.Run(power, leftCount, rightCount) ? 0 : 1;
		}
	}

	// Аргументы в командной строке некорректны
	aux::Printc("#12Error: invalid command line\n");
	return 1;
}
