﻿//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

namespace lab {

// Главная функция эксперимента
int PalindromicDatesMain(int argCount, const wchar_t* args[]);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Date - вспомогательный класс для работы с датами
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct Date
{
	int year;			// Год (наша эра, от 1)
	int month;			// Месяц: от 1 (январь) до 12 (декабрь)
	int day;			// День: от 1 до 31
	int dayOfTheWeek;	// День недели: от 0 (воскресенье) до 6 (суббота)

public:
	// Устанавливает значение даты на понедельник 1 января 1 года. Это
	// значение соответствует результату вызова функции Decode(0)
	void Clear();

	// Декодирует указанное количество полных дней, прошедших
	// с полуночи 1 января 1 года и обновляет поля структуры
	void Decode(int days);
	// Вычисляет количество дней, прошедших с полуночи 1 января
	// 1 года до полуночи даты, указанной в полях структуры
	int Encode() const;

	// Возвращает true, если year - високосный год
	static bool IsLeapYear(int year);
};

} // namespace lab
