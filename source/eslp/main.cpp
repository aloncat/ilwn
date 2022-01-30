//∙ESLP/iLWN
// Copyright (C) 2022-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"

#include "diophantine.h"

#include <auxlib/print.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();

	aux::Printc(L"Equal Sums of Like Powers (ESLP)/iLWN. Standalone app. Ver. 0.11a\n"
		"#8For more information, please visit us at #9https://dmaslov.me/eslp/\n");

	// Программа принимает 2 обязательных и 1 опциональный аргумент командной строки: 1) степень уравнения,
	// 2) количество слагаемых в его правой части, 3) опциональное значение самого старшего коэффициента
	// (в левой части), с которого будет начат поиск (при его отсутствии будет использовано значение 2)

	if (argCount <= 1)
	{
		// Справка для пустой командной строки
		aux::Printc("#15Usage: #6eslp.exe#7 power count [factor]\n");
		return 0;
	}

	if (argCount == 3 || argCount == 4)
	{
		int power = wcstol(args[1], nullptr, 10);
		int count = wcstol(args[2], nullptr, 10);
		int hiFactor = (argCount >= 4) ? wcstol(args[3], nullptr, 10) : 2;

		if (power >= 1 && power <= 9 && count >= 2 && count <= 50 && hiFactor >= 0)
		{
			// Ищем коэффициенты. При ошибке функция вернёт false
			if (SearchForFactors(power, count, hiFactor))
				return 0;

			// Что-то пошло не так. Сообщение об ошибке уже выведено
			return 1;
		}
	}

	// Аргументы в командной строке некорректны
	aux::Printc("#12Error: invalid command line\n");
	return 1;
}
