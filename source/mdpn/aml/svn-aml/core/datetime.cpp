#include "datetime.h"

#include "../../core/winapi.h"

namespace util {

static const int monthA[16] = { -1, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333 };

//----------------------------------------------------------------------------------------------------------------------
void DateTime::Clear()
{
	year = 1601;
	month = 1;
	day = 1;
	dayOfWeek = 1;
	hour = 0;
	minute = 0;
	second = 0;
	ms = 0;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::Date(bool UTC)
{
	uint64_t time = Now(UTC);
	return time - (time % 86400000);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::Decode(uint64_t time)
{
	SetDate(static_cast<unsigned>(time / 86400000));
	SetTime(time % 86400000);
}

//----------------------------------------------------------------------------------------------------------------------
void DateTime::DecodeDate(uint64_t time)
{
	SetDate(static_cast<unsigned>(time / 86400000));
}

//----------------------------------------------------------------------------------------------------------------------
void DateTime::DecodeTime(uint64_t time)
{
	SetTime(time % 86400000);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::Encode()
{
	return EncodeDate() + EncodeTime();
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::EncodeDate()
{
	unsigned y = year - 1601;
	unsigned leapYearC = y >> 2;
	unsigned days = y * 365 + leapYearC - leapYearC / 25 + ((leapYearC / 25) >> 2);
	days += monthA[month & 15] + day;

	if ((month > 2) && !(year & 3) && ((year % 100 != 0) || (year % 400 == 0)))
		++days;

	return days * 86400000ll;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::EncodeTime()
{
	return ((60 * hour + minute) * 60 + second) * 1000 + ms;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::FromUNIX(int64_t time)
{
	return (time + 11644473600ll) * 1000;
}

//----------------------------------------------------------------------------------------------------------------------
bool DateTime::IsLeapYear(unsigned year)
{
	if (year & 3) return false;

	unsigned k = year / 100;
	return (year - 100 * k) || !(k & 3);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::Now(bool UTC)
{
	#if AML_OS_WINDOWS
		FILETIME system, t;
		::GetSystemTimeAsFileTime(&system);
		if (UTC || (::FileTimeToLocalFileTime(&system, &t) == 0)) t = system;
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::SetDate(unsigned totalDays)
{
	dayOfWeek = (totalDays + 1) % 7;

	unsigned y400 = totalDays / 146097;
	totalDays -= y400 * 146097;
	year = static_cast<uint16_t>(1601 + y400 * 400);
	for (int i = 3; i && (totalDays >= 36524); --i)
	{
		totalDays -= 36524;
		year += 100;
	}
	unsigned y4 = totalDays / 1461;
	totalDays -= y4 * 1461;
	year += static_cast<uint16_t>(y4 << 2);
	for (int i = 3; i && (totalDays >= 365); --i)
	{
		totalDays -= 365;
		++year;
	}

	unsigned janFeb = 59;
	if (!(year & 3) && ((year % 100 != 0) || (year % 400 == 0)))
	{
		if (totalDays >= 60)
			--totalDays;
		else
			++janFeb;
	}
	if (totalDays < 181)
	{
		if (totalDays < 90)
			month = (totalDays < 31) ? 1 : (totalDays < janFeb) ? 2 : 3;
		else
			month = (totalDays < 120) ? 4 : (totalDays < 151) ? 5 : 6;
	}
	else if (totalDays < 273)
		month = (totalDays < 212) ? 7 : (totalDays < 243) ? 8 : 9;
	else
		month = (totalDays < 304) ? 10 : (totalDays < 334) ? 11 : 12;

	day = static_cast<uint16_t>(totalDays - monthA[month]);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::SetTime(unsigned milliseconds)
{
	unsigned h = milliseconds / 3600000;
	hour = static_cast<uint16_t>(h);
	milliseconds -= h * 3600000;

	unsigned m = milliseconds / 60000;
	minute = static_cast<uint16_t>(m);
	milliseconds -= m * 60000;

	second = static_cast<uint16_t>(milliseconds / 1000);
	ms = static_cast<uint16_t>(milliseconds % 1000);
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::Time(bool UTC)
{
	uint64_t time = Now(UTC);
	return time % 86400000;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::ToLocal(uint64_t time)
{
	#if AML_OS_WINDOWS
		time *= 10000;
		FILETIME system, t;
		system.dwHighDateTime = time >> 32;
		system.dwLowDateTime = static_cast<uint32_t>(time);
		if (::FileTimeToLocalFileTime(&system, &t) == 0) return 0;
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
int64_t DateTime::ToUNIX(uint64_t time)
{
	return time / 1000 - 11644473600ll;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::Update(bool UTC)
{
	uint64_t time = Now(UTC);
	SetDate(static_cast<unsigned>(time / 86400000));
	SetTime(time % 86400000);
}

//----------------------------------------------------------------------------------------------------------------------
void DateTime::UpdateDate(bool UTC)
{
	uint64_t time = Now(UTC);
	SetDate(static_cast<unsigned>(time / 86400000));
}

//----------------------------------------------------------------------------------------------------------------------
void DateTime::UpdateTime(bool UTC)
{
	uint64_t time = Now(UTC);
	SetTime(time % 86400000);
}

} // namespace util
