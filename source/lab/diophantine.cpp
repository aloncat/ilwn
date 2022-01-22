//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "diophantine.h"

#include <map>

namespace lab {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск коэффициентов Диофантова уравнения вида p.1.n (наивное решение)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
unsigned CalcPowers(unsigned high, int power, int count, uint64_t* powers)
{
	memset(powers, 0, 8 * (high + 1));

	for (unsigned i = 1; i <= high; ++i)
	{
		uint64_t o, p = 1, c = i * count;

		for (int j = 0; j < power; ++j)
		{
			o = p;
			p *= i;

			// Проверяем, не возникло ли переполнение при умножении, а также, что оно не возникнет позже при сложении
			// count слагаемых уравнения. Если переполнение возникло, значит для текущего i значение в степени power,
			// умноженное на count, не помещается в 64 бита, и мы должны прервать цикл, вернув предыдущее значение i
			// как максимально возможное значение коэффициента
			if ((p * count) / c != o)
				return i - 1;
		}

		powers[i] = p;
	}

	return high;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchForFactors(int power, int count, unsigned hiFactor)
{
	constexpr int MAX_COUNT = 50;
	constexpr unsigned MAX_FACTOR = 9999;

	if (power < 2 || power > 9 || count < 2 || count > MAX_COUNT)
		return false;

	printf("Searching for factors (%i.1.%i)\n", power, count);

	uint64_t* powers = new uint64_t[MAX_FACTOR + 1];
	const unsigned maxFactor = CalcPowers(MAX_FACTOR, power, count, powers);
	printf("Factor limit is set to %u\n", maxFactor);

	// Массив k хранит коэффициенты уравнения (начиная с индекса 1). В элементе
	// с индексом 0 будем хранить максимально возможное значение коэффициента
	unsigned k[1 + MAX_COUNT] = { maxFactor };
	k[1] = (hiFactor > 1) ? hiFactor : 2;
	for (int i = 1; i < count; ++i)
		k[i + 1] = 1;

	std::map<uint64_t, unsigned> results;
	// В results поместим все возможные значения частей уравнения
	// и соответствующие им значения коэффициента в левой части
	for (unsigned i = hiFactor; i <= maxFactor; ++i)
		results.emplace(powers[i], i);

	size_t solutionsFound = 0;
	while (k[1] <= maxFactor)
	{
		uint64_t sum = 0;
		// Вычисляем сумму в правой части
		for (int i = 0; i < count; ++i)
			sum += powers[k[i + 1]];

		// Ищем подходящий коэффициент для левой части
		if (auto it = results.find(sum); it != results.end())
		{
			// Если нашли 100 решений, заканчиваем работу
			if (++solutionsFound >= 100)
				break;

			// Формируем строку с коэффициентами
			auto s = std::to_string(it->second);
			s += "=" + std::to_string(k[1]);
			for (int i = 1; i < count; ++i)
			{
				s += "+" + std::to_string(k[i + 1]);
			}

			// И выводим её на экран
			printf("Solution found: %s\n", s.c_str());
		}

		// Переходим к следующему набору коэффициентов правой части, перебирая все возможные
		// комбинации так, чтобы коэффициенты всегда располагались в невозрастающем порядке
		for (int i = count; i > 0; --i)
		{
			if (k[i] < k[i - 1])
			{
				++k[i];
				break;
			}
			k[i] = 1;
		}
	}

	delete[] powers;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
int DiophantineMain(int argCount, const wchar_t* args[])
{
	// Программа принимает 2 обязательных и 1 опциональный аргумент командной строки: 1) степень уравнения,
	// 2) количество слагаемых в его правой части, 3) опциональное значение самого старшего коэффициента, с
	// которого будет начат поиск (если аргумент не указан, то будет использовано значение 2)

	if (argCount == 3 || argCount == 4)
	{
		int power = wcstol(args[1], nullptr, 10);
		int count = wcstol(args[2], nullptr, 10);
		int hiFactor = (argCount >= 4) ? wcstol(args[3], nullptr, 10) : 2;

		if (power >= 2 && power <= 9 && count >= 2 && count <= 50 && hiFactor >= 1)
		{
			// Ищем коэффициенты. При ошибке функция вернёт false
			if (SearchForFactors(power, count, hiFactor))
				return 0;

			// Что-то пошло не так. Сообщение об ошибке уже выведено
			return 1;
		}
	}

	// Аргументы в командной строке некорректны
	printf("Error: invalid command line\n");
	return 1;
}

} // namespace lab
