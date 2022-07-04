//∙Lab/iLWN
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "palindromicdates.h"

#include <auxlib/print.h>
#include <core/debug.h>
#include <core/file.h>
#include <core/strformat.h>
#include <core/strutil.h>

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
		if (dayOfTheWeek == 1)
		{
			const bool isLeapYear = Date::IsLeapYear(year);
			aux::Printf("January 1, %i is Monday%s\n", year,
				isLeapYear ? "#6 (leap)" : "");
		}

		const int days = Date::IsLeapYear(year - 1) ? 366 : 365;
		if (dayOfTheWeek -= days % 7; dayOfTheWeek < 0)
			dayOfTheWeek += 7;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Подсчёт количества лет, 1 января которых приходится на разные дни недели
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* const dayOfTheWeekNames[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

//--------------------------------------------------------------------------------------------------------------------------------
void CountDaysOfTheWeek()
{
	int year = 2022;		// Год (наша эра, от 1)
	int dayOfTheWeek = 6;	// День недели: от 0 (воскресенье) до 6 (суббота)

	int leapYears[7] = {};
	int nonLeapYears[7] = {};

	for (; year > 0; --year)
	{
		// Так как високосные годы влияют на количество дней недели, и каждые 400 лет весь
		// цикл повторяется, то мы будем считать только годы, начиная от 1-го и до 2000-го
		if (year <= 2000)
		{
			if (Date::IsLeapYear(year))
				++leapYears[dayOfTheWeek];
			else
				++nonLeapYears[dayOfTheWeek];
		}

		const int days = Date::IsLeapYear(year - 1) ? 366 : 365;
		if (dayOfTheWeek -= days % 7; dayOfTheWeek < 0)
			dayOfTheWeek += 7;
	}

	for (int i = 0; i < 7; ++i)
	{
		const int total = leapYears[i] + nonLeapYears[i];
		aux::Printf("#11%s#7: %i leap years + %i non-leap years, %i total\n",
			dayOfTheWeekNames[i], leapYears[i], nonLeapYears[i], total);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Date (класс)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int Date::s_Months[16] = { -1, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333 };

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

	day = days - s_Months[month];
}

//--------------------------------------------------------------------------------------------------------------------------------
int Date::Encode() const
{
	int y = year - 1;
	int leapYears = y >> 2;
	leapYears = leapYears - leapYears / 25 + ((leapYears / 25) >> 2);
	int days = y * 365 + leapYears + s_Months[month & 15] + day;

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

const wchar_t* monthNames[16] = { L"", L"Январь", L"Февраль", L"Март", L"Апрель", L"Май",
	L"Июнь", L"Июль", L"Август", L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь" };

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
struct DateString
{
	std::string terms[3];

	DateString() = default;
	DateString(const char* format, int a, int b, int c)
	{
		terms[0] = util::Format((format[0] == '2') ? "%02i" : "%i", a);
		terms[1] = util::Format((format[1] == '2') ? "%02i" : "%i", b);
		terms[2] = util::Format((format[2] == '2') ? "%02i" : "%i", c);
	}

	bool IsPalindrome() const
	{
		if (const auto s = ToString(); !s.empty())
		{
			for (size_t left = 0, right = s.size() - 1; left < right; ++left, --right)
			{
				if (s[left] != s[right])
					return false;
			}
			return true;
		}
		return false;
	}

	std::string ToString() const
	{
		return terms[0] + terms[1] + terms[2];
	}
};

//--------------------------------------------------------------------------------------------------------------------------------
struct DateInfo
{
	DateType type = DateType::NoPalindome;	// Тип палиндромной даты
	DateString dateString;					// Строка с записанной датой
	std::string format;						// Формат записанной даты

	int dateDay = 0;						// День (от 1 января 1 года)
	int dayOfTheYear = 0;					// Номер дня года (от 1)
	int daysBeforeNewYear = 0;				// Дней до Нового года
	bool isInPalindromicWeek = false;		// true, если входит в палиндромную неделю

	bool operator <(int rhs) const { return dateDay < rhs; }
	bool operator <(const DateInfo& rhs) const { return dateDay < rhs.dateDay; }
	friend bool operator <(int lhs, const DateInfo& rhs) { return lhs < rhs.dateDay; }
};

//--------------------------------------------------------------------------------------------------------------------------------
static DateInfo GetDateInfo(const Date& date)
{
	DateInfo result;

	// Формат YMD
	DateString ymdNormal("---", date.year, date.month, date.day);
	DateString ymdExtended("222", date.year, date.month, date.day);
	bool ymdPalindrome = ymdNormal.IsPalindrome() || ymdExtended.IsPalindrome();

	// Формат DMY
	DateString dmyNormal("---", date.day, date.month, date.year);
	DateString dmyExtended("222", date.day, date.month, date.year);
	bool dmyPalindrome = dmyNormal.IsPalindrome() || dmyExtended.IsPalindrome();

	// Формат MDY
	DateString mdyNormal("---", date.month, date.day, date.year);
	DateString mdyExtended("222", date.month, date.day, date.year);
	bool mdyPalindrome = mdyNormal.IsPalindrome() || mdyExtended.IsPalindrome();

	// Всемирный палиндромный день (вывод в YMD, высший приоритет)
	if (ymdPalindrome && dmyPalindrome && mdyPalindrome)
	{
		result.type = DateType::WorldWide;
		bool useExtended = ymdExtended.IsPalindrome();
		result.dateString = useExtended ? ymdExtended : ymdNormal;
		result.format = useExtended ? "yyyy-mm-dd" : "yyyy-m-d";
	}
	// Extended/Normal YMD
	else if (ymdPalindrome)
	{
		bool useExtended = ymdExtended.IsPalindrome();
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? ymdExtended : ymdNormal;
		result.format = useExtended ? "yyyy-mm-dd" : "yyyy-m-d";
	}
	// Extended/Normal DMY
	else if (dmyPalindrome)
	{
		bool useExtended = dmyExtended.IsPalindrome();
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? dmyExtended : dmyNormal;
		result.format = useExtended ? "dd-mm-yyyy" : "d-m-yyyy";
	}
	// Extended/Normal MDY
	else if (mdyPalindrome)
	{
		bool useExtended = mdyExtended.IsPalindrome();
		result.type = useExtended ? DateType::Extended : DateType::Normal;
		result.dateString = useExtended ? mdyExtended : mdyNormal;
		result.format = useExtended ? "mm-dd-yyyy" : "m-d-yyyy";
	}
	// Проверяем форматы с сокращённым годом (низший приоритет)
	else if (date.year >= 100)
	{
		const int year = date.year % 100;
		DateString ymdRedY1("2--", year, date.month, date.day);
		DateString ymdRedY2("222", year, date.month, date.day);
		DateString dmyRedY1("--2", date.day, date.month, year);
		DateString dmyRedY2("222", date.day, date.month, year);
		DateString mdyRedY1("--2", date.month, date.day, year);
		DateString mdyRedY2("222", date.month, date.day, year);

		if (ymdRedY1.IsPalindrome() || ymdRedY2.IsPalindrome())
		{
			result.type = DateType::ReducedYear;
			bool useExtended = ymdRedY2.IsPalindrome();
			result.dateString = useExtended ? ymdRedY2 : ymdRedY1;
			result.format = useExtended ? "yy-mm-dd" : "yy-m-d";
		}
		else if (dmyRedY1.IsPalindrome() || dmyRedY2.IsPalindrome())
		{
			result.type = DateType::ReducedYear;
			bool useExtended = dmyRedY2.IsPalindrome();
			result.dateString = useExtended ? dmyRedY2 : dmyRedY1;
			result.format = useExtended ? "dd-mm-yy" : "d-m-yy";
		}
		else if (mdyRedY1.IsPalindrome() || mdyRedY2.IsPalindrome())
		{
			result.type = DateType::ReducedYear;
			bool useExtended = mdyRedY2.IsPalindrome();
			result.dateString = useExtended ? mdyRedY2 : mdyRedY1;
			result.format = useExtended ? "mm-dd-yy" : "m-d-yy";
		}
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void WriteTable(util::File& file, int year, bool isHeader)
{
	std::wstring text;

	if (isHeader)
	{
		text += util::Format(L""
			"<div id=\"%i\" class=\"column\">\n"
			"  <table class=\"paldates small\">\n",
			year);

		text += util::Format(L""
			"\t<tr>\n"
			"\t  <td colspan=6><b>%i год</b>%s</td>\n"
			"\t</tr>\n",
			year,
			Date::IsLeapYear(year) ? L" (високосный)" : L"");

		text += L""
			"\t<tr>\n"
			"\t  <td>Дата</td>\n"
			"\t  <td>Тип</td>\n"
			"\t  <td>Формат</td>\n"
			"\t  <td>Палиндром</td>\n"
			"\t  <td>День от НГ</td>\n"
			"\t  <td>Дней до НГ</td>\n"
			"\t</tr>\n";
	} else
	{
		text += L""
			"  </table>\n"
			"</div>\n\n";
	}

	auto utf8 = util::ToUtf8(text);
	file.Write(utf8.data(), utf8.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
static void WriteRow(util::File& file, const DateInfo& info, const Date& date)
{
	std::string format, dateString;
	const auto terms = util::Split(info.format, "-");

	Assert(terms.size() == 3);
	for (int i = 0; i < 3; ++i)
	{
		std::string span = "<span class=\"";
		if (terms[i][0] == 'y')
			span += "year";
		else if (terms[i][0] == 'm')
			span += "month";
		else
			span += "day";
		span += "\">";

		format += (i ? "-" : "") + span + terms[i] + "</span>";
		dateString += span + info.dateString.terms[i] + "</span>";
	}

	std::wstring tdClass = (info.type >= DateType::Normal) ?
		util::Format(L" class=\"t%i\"", info.type) : L"";

	auto text = util::Format(L""
		"\t<tr>\n"
		"\t  <td%s>%s, %i%s</td>\n"
		"\t  <td%s>T%i</td>\n"
		"\t  <td>%s</td>\n"
		"\t  <td>%s</td>\n"
		"\t  <td>%i</td>\n"
		"\t  <td>%i</td>\n"
		"\t</tr>\n",
		tdClass.c_str(), monthNames[date.month], date.day,
		info.isInPalindromicWeek ? L" <div class=\"week\">нед.</div>" : L"",
		tdClass.c_str(), info.type,
		util::FromAnsi(format).c_str(),
		util::FromAnsi(dateString).c_str(),
		info.dayOfTheYear,
		info.daysBeforeNewYear);

	auto utf8 = util::ToUtf8(text);
	file.Write(utf8.data(), utf8.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
static void PrintDay(const Date& date, const DateInfo& info)
{
	static const int colors[8] = { 8, 8, 9, 15, 10 };
	const int color = colors[static_cast<int>(info.type) & 7];

	aux::Printf("#%iYMD %04i-%02i-%02i T%i '%s' %s (##%i, %i until NY)\n", color,
		date.year, date.month, date.day, info.type, info.dateString.ToString().c_str(),
		info.format.c_str(), info.dayOfTheYear, info.daysBeforeNewYear);
}

//--------------------------------------------------------------------------------------------------------------------------------
static void TestDates(bool writeToFile = false)
{
	std::set<DateInfo, std::less<void>> allDays;
	Date date, newYear, weekEnd;

	newYear.day = date.day = 1;
	newYear.month = date.month = 1;
	date.year = 2001;

	int daysInARow = 0;
	auto day = date.Encode();
	while (date.year <= 2030)
	{
		auto info = GetDateInfo(date);
		if (info.type > DateType::NoPalindome)
		{
			++daysInARow;
			info.dateDay = day;

			newYear.year = date.year;
			info.dayOfTheYear = 1 + day - newYear.Encode();

			++newYear.year;
			info.daysBeforeNewYear = newYear.Encode() - day;

			PrintDay(date, info);
			allDays.insert(std::move(info));
		} else
		{
			if (daysInARow > 2)
			{
				for (int i = day - daysInARow; i < day; ++i)
				{
					if (auto it = allDays.find(i); it != allDays.end())
					{
						const_cast<DateInfo&>(*it).isInPalindromicWeek = true;
					}
				}

				date.Decode(day - daysInARow);
				weekEnd.Decode(day - 1);

				aux::Printf("#6Week: %i days in a row, YMD %04i-%02i-%02i -> %04i-%02i-%02i\n", daysInARow,
					date.year, date.month, date.day, weekEnd.year, weekEnd.month, weekEnd.day);
			}

			daysInARow = 0;
		}

		date.Decode(++day);
	}

	if (writeToFile)
	{
		util::BinaryFile f;
		if (f.Open(L"table.html", util::FILE_CREATE_ALWAYS | util::FILE_OPEN_WRITE))
		{
			int lastYear = 0;
			for (const auto& info : allDays)
			{
				date.Decode(info.dateDay);
				if (date.year != lastYear)
				{
					if (lastYear)
					{
						WriteTable(f, 0, false);
					}

					WriteTable(f, date.year, true);
					lastYear = date.year;
				}

				WriteRow(f, info, date);
			}

			if (lastYear)
			{
				WriteTable(f, 0, false);
			}

			f.Close();
		}
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
	FindAllMondays();
	CountDaysOfTheWeek();
	TestDates();

	return 0;
}

} // namespace lab
