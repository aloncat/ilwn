﻿//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "palindromicdates.h"

#include <auxlib/print.h>
#include <core/strformat.h>

namespace lab {

// Существует несколько основных (наиболее распространённых) способов записи дат. В качестве разделителя в разных
// странах используется точка (.), дефис (-) либо косая черта (/). Во всех случаях (кроме ISO 8601) год может
// записываться либо полностью (до 4 цифр), либо 2 последними цифрами. Ниже перечислены возможные форматы:
//   1) dd-mm-yyyy и сокращённая форма dd-mm-yy (Россия, многие страны Европы)
//   2) d-m-yyyy и сокращённая форма d-m-yy (некоторые страны Европы)
//   3) mm-dd-yyyy, m-d-yyyy, а также формы mm-dd-yy и m-d-yy (США)
//   4) yyyy-mm-dd (некоторые страны Европы, Япония, ISO 8601)
//   5) yyyy-m-d (КНР, Гонконг, Тайвань)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск всех лет, в которых 1 января - понедельник (по григорианскому календарю)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Григорианский календарь был введён в 1582 году взамен прежнего юлианского. Они отличаются високосными годами. В
// григорианском календаре високосный год - каждый 4-й, кроме каждого 100-го, но также каждый 400-й. Таким образом,
// на каждые 400 лет приходится 97 високосных, а количество дней составляет 146097=7*20871, то есть в 400 годах
// ровно 20871 полная неделя, поэтому 1-й год, 401-й, 801-й, 1201-й, 1601-й и 2001-й начинаются с понедельника

//--------------------------------------------------------------------------------------------------------------------------------
void FindAllMondays()
{
	// Сегодня 23 января 2022 года. И я точно знаю, что этот день - воскресенье. Не сложно
	// подсчитать, что 1 января этого года - суббота, так как это 23-1=22=3*7+1 дня назад

	int year = 2022;		// Год (наша эра, от 1)
	int dayOfTheWeek = 6;	// День недели: от 0 (воскресенье) до 6 (суббота)

	for (; year > 0; --year)
	{
		const int days = Date::IsLeapYear(year) ? 366 : 365;
		if (dayOfTheWeek -= days % 7; dayOfTheWeek < 0)
			dayOfTheWeek += 7;

		if (dayOfTheWeek == 1)
		{
			aux::Printf("January 1, %i is Monday\n", year);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Подсчёт количества лет, 1 января которых приходится на разные дни недели
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* const dayOfTheWeekNames[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

//--------------------------------------------------------------------------------------------------------------------------------
void CountFirstDaysOfTheWeek()
{
	int year = 2022;		// Год (наша эра, от 1)
	int dayOfTheWeek = 6;	// День недели: от 0 (воскресенье) до 6 (суббота)

	int leapYears[7] = {};
	int nonLeapYears[7] = {};

	for (; year > 0; --year)
	{
		const bool isLeapYear = Date::IsLeapYear(year);

		// Так как високосные годы влияют на количество дней недели, и каждые 400 лет весь
		// цикл повторяется, то мы будем считать только годы, начиная от 1-го и до 2000-го
		if (year <= 2000)
		{
			if (isLeapYear)
				++leapYears[dayOfTheWeek];
			else
				++nonLeapYears[dayOfTheWeek];
		}

		const int days = isLeapYear ? 366 : 365;
		if (dayOfTheWeek -= days % 7; dayOfTheWeek < 0)
			dayOfTheWeek += 7;
	}

	for (int i = 0; i < 7; ++i)
	{
		const int total = leapYears[i] + nonLeapYears[i];
		aux::Printf("%s: %i leap years + %i non-leap years, %i total\n",
			dayOfTheWeekNames[i], leapYears[i], nonLeapYears[i], total);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Date (класс)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int months[16] = { -1, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333 };

//--------------------------------------------------------------------------------------------------------------------------------
void Date::Clear()
{
	year = 1;
	month = 1;
	day = 1;
	dayOfTheWeek = 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Date::Decode(int days)
{
	dayOfTheWeek = (days + 1) % 7;

	unsigned y400 = days / 146097;
	days -= y400 * 146097;
	year = 1 + y400 * 400;

	for (int i = 3; i && days >= 36524; --i)
	{
		days -= 36524;
		year += 100;
	}

	int y4 = days / 1461;
	days -= y4 * 1461;
	year += y4 << 2;

	for (int i = 3; i && days >= 365; --i)
	{
		days -= 365;
		++year;
	}

	int janFeb = 59;
	if (IsLeapYear(year))
	{
		if (days >= 60)
			--days;
		else
			++janFeb;
	}

	if (days < 181)
	{
		if (days < 90)
			month = (days < 31) ? 1 : (days < janFeb) ? 2 : 3;
		else
			month = (days < 120) ? 4 : (days < 151) ? 5 : 6;
	}
	else if (days < 273)
		month = (days < 212) ? 7 : (days < 243) ? 8 : 9;
	else
		month = (days < 304) ? 10 : (days < 334) ? 11 : 12;

	day = days - months[month];
}

//--------------------------------------------------------------------------------------------------------------------------------
int Date::Encode() const
{
	int y = year - 1;
	int leapYears = y >> 2;
	leapYears = leapYears - leapYears / 25 + ((leapYears / 25) >> 2);
	int days = y * 365 + leapYears + months[month & 15] + day;

	if (month > 2 && IsLeapYear(year))
		++days;

	return days;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Date::IsLeapYear(int year)
{
	return !(year & 3) && (year % 100 || !(year % 400));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Поиск палиндромных дат
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// В рамках эксперимента по поиску палиндромных дат мы придерживаемся нескольких правил. Порядок записи дня, месяца и года
// в дате может быть только DMY, MDY или YMD. Дата всемирного палиндромного дня обязательно должна являться палиндромом во
// всех трёх случаях. Значения дня и месяца от 1 до 9 могут записываться как 1 цифрой, так и 2 (дополняется нолём), но это
// должно выполняться одинаковым образом и для дня, и для месяца. То есть 3 мая 2008 года может быть записано как "3508"
// или "030508", но не как "30508" или "03508". Для 12 апреля 2020 года корректны будут записи: "12042020" (dd-mm-yyyy),
// "12420" (d-m-yy), "41220" (m-d-yy), "2020412" (yyyy-m-d) и другие. Запись года может быть сокращёна до последних 2
// цифр (только для годов, начиная с 100-го) или быть полной (1-4 цифры года). Если год равен от 1 до 9 и день/месяц
// дополняются нолями, то год также должен дополняться нолём. Для всемирного палиндромного дня год не сокращается

//--------------------------------------------------------------------------------------------------------------------------------
enum class DateType
{
	NoPalindome,	// Дата не является палиндромом ни в одной форме
	ReducedYear,	// Палиндромная дата, сокращённый год (2 последние цифры из 3-4)
	Normal,			// Палиндромная дата, год полный (1-4 цифры), значения 1-9 для месяца и/или дня не дополняются нолем
	Extended,		// Палиндромная дата из 6-8 цифр: день и месяц записаны 2, а год 2-4 цифрами
	WorldWide		// Всемирный палиндромный день (см. комментарий выше)
};

//--------------------------------------------------------------------------------------------------------------------------------
struct DateInfo
{
	DateType type = DateType::NoPalindome;	// Тип палиндромной даты
	std::string dateString;					// Строка с записанной датой
	std::string format;						// Формат записанной даты
};

//--------------------------------------------------------------------------------------------------------------------------------
static bool IsPalindrome(std::string_view s)
{
	if (s.empty())
		return false;

	for (size_t left = 0, right = s.size() - 1; left < right; ++left, --right)
	{
		if (s[left] != s[right])
			return false;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
static DateInfo GetDateInfo(const Date& date)
{
	DateInfo result;

	// Формат YMD
	auto ymdNormal = util::Format("%i%i%i", date.year, date.month, date.day);
	auto ymdExtended = util::Format("%02i%02i%02i", date.year, date.month, date.day);
	bool ymdPalindrome = IsPalindrome(ymdNormal) || IsPalindrome(ymdExtended);

	// Формат DMY
	auto dmyNormal = util::Format("%i%i%i", date.day, date.month, date.year);
	auto dmyExtended = util::Format("%02i%02i%02i", date.day, date.month, date.year);
	bool dmyPalindrome = IsPalindrome(dmyNormal) || IsPalindrome(dmyExtended);

	// Формат MDY
	auto mdyNormal = util::Format("%i%i%i", date.month, date.day, date.year);
	auto mdyExtended = util::Format("%02i%02i%02i", date.month, date.day, date.year);
	bool mdyPalindrome = IsPalindrome(mdyNormal) || IsPalindrome(mdyExtended);

	// Всемирный палиндромный день (вывод в YMD, высший приоритет)
	if (ymdPalindrome && dmyPalindrome && mdyPalindrome)
	{
		result.type = DateType::WorldWide;
		bool useExtended = IsPalindrome(ymdExtended);
		result.dateString = useExtended ? ymdExtended : ymdNormal;
		result.format = useExtended ? "yyyy-mm-dd" : "yyyy-m-d";
	}
	// Extended/Normal YMD
	else if (ymdPalindrome)
	{
		bool useExtended = IsPalindrome(ymdExtended);
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? ymdExtended : ymdNormal;
		result.format = useExtended ? "yyyy-mm-dd" : "yyyy-m-d";
	}
	// Extended/Normal DMY
	else if (dmyPalindrome)
	{
		bool useExtended = IsPalindrome(dmyExtended);
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? dmyExtended : dmyNormal;
		result.format = useExtended ? "dd-mm-yyyy" : "d-m-yyyy";
	}
	// Extended/Normal MDY
	else if (mdyPalindrome)
	{
		bool useExtended = IsPalindrome(mdyExtended);
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? mdyExtended : mdyNormal;
		result.format = useExtended ? "mm-dd-yyyy" : "m-d-yyyy";
	}
	// Проверяем форматы с сокращённым годом (низший приоритет)
	else if (date.year >= 100)
	{
		const int year = date.year % 100;
		auto ymdRedY1 = util::Format("%02i%i%i", year, date.month, date.day);
		auto ymdRedY2 = util::Format("%02i%02i%02i", year, date.month, date.day);
		auto dmyRedY1 = util::Format("%i%i%02i", date.day, date.month, year);
		auto dmyRedY2 = util::Format("%02i%02i%02i", date.day, date.month, year);
		auto mdyRedY1 = util::Format("%i%i%02i", date.month, date.day, year);
		auto mdyRedY2 = util::Format("%02i%02i%02i", date.month, date.day, year);

		if (IsPalindrome(ymdRedY1) || IsPalindrome(ymdRedY2))
		{
			result.type = DateType::ReducedYear;
			result.dateString = IsPalindrome(ymdRedY2) ? ymdRedY2 : ymdRedY1;
			result.format = IsPalindrome(ymdRedY2) ? "yy-mm-dd" : "yy-m-d";
		}
		else if (IsPalindrome(dmyRedY1) || IsPalindrome(dmyRedY2))
		{
			result.type = DateType::ReducedYear;
			result.dateString = IsPalindrome(dmyRedY2) ? dmyRedY2 : dmyRedY1;
			result.format = IsPalindrome(dmyRedY2) ? "dd-mm-yy" : "d-m-yy";
		}
		else if (IsPalindrome(mdyRedY1) || IsPalindrome(mdyRedY2))
		{
			result.type = DateType::ReducedYear;
			result.dateString = IsPalindrome(mdyRedY2) ? mdyRedY2 : mdyRedY1;
			result.format = IsPalindrome(mdyRedY2) ? "mm-dd-yy" : "m-d-yy";
		}
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void TestDates()
{
	Date date;

	date.day = 1;
	date.month = 1;
	date.year = 2020;

	auto day = date.Encode();
	while (date.year <= 2030)
	{
		auto info = GetDateInfo(date);
		if (info.type > DateType::NoPalindome)
		{
			const int colors[8] = { 8, 8, 9, 15, 10 };
			const int color = colors[static_cast<int>(info.type) & 7];

			aux::Printf("#%iYMD %04i-%02i-%02i T%i '%s' %s\n",
				color, date.year, date.month, date.day, info.type,
				info.dateString.c_str(), info.format.c_str());
		}

		date.Decode(++day);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Главная функция эксперимента
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
int PalindromicDatesMain(int argCount, const wchar_t* args[])
{
	//FindAllMondays();
	//CountFirstDaysOfTheWeek();
	TestDates();

	return 0;
}

} // namespace lab