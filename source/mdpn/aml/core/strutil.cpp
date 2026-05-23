//⬪AML⬪
#include "pch.h"
#include "strutil.h"

#include "array.h"
#include "platform.h"
#include "winapi.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8_t loCaseTTA[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32 };

static const uint8_t upCaseTTA[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32 };

//----------------------------------------------------------------------------------------------------------------------
int StrCmp(const char* pStrA, const char* pStrB)
{
	// TODO: возможно, есть смысл попытаться написать свою реализацию этой функции (например, в случае, если
	// строки выровнены, сравнивать по 4/8 байт за раз, что должно быть быстрее - но тут важно, как хорошо
	// функции реализованы на других платформах, чтобы не сделать хуже); если своей реализации не будет,
	// то лучше перенести тело функции в заголовок
	return strcmp(pStrA, pStrB);
}

//----------------------------------------------------------------------------------------------------------------------
int StrCmp(const wchar_t* pStrA, const wchar_t* pStrB)
{
	// TODO: см. комментарий к StrCmp/strcmp
	return wcscmp(pStrA, pStrB);
}

//----------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const char* pStrA, const char* pStrB)
{
	int a, b;
	do {
		a = static_cast<unsigned char>(*pStrA++);
		a += loCaseTTA[a];
		b = static_cast<unsigned char>(*pStrB++);
		b += loCaseTTA[b];
	} while (b && a == b);
	return a - b;
}

//----------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const wchar_t* pStrA, const wchar_t* pStrB)
{
	// В целях оптимизации мы полагаем, что тип wchar_t беззнаковый. Если это не так и при
	// этом размер типа wchar_t меньше размера переменных a и b, то функция может работать
	// некорректно, если строки содержат символы, у которых значения старших битов равны 1
	static_assert(std::is_unsigned<wchar_t>::value || sizeof(wchar_t) == sizeof(unsigned),
		"Type wchar_t must be unsigned or same-sized as unsigned (int) type");

	unsigned a, b;
	do {
		a = *pStrA++;
		if (a <= 0x7f)
			a += loCaseTTA[a];
		b = *pStrB++;
		if (b <= 0x7f)
			b += loCaseTTA[b];
	} while (a && a == b);

	return (sizeof(wchar_t) < sizeof(a)) ? static_cast<int>(a - b) :
		(a == b) ? 0 : (a < b) ? -1 : 1;
}

//----------------------------------------------------------------------------------------------------------------------
int StrCaseCmp(const char* pStrA, const char* pStrB)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Нужна своя реализация: цикл сравнения с приведением
		// каждого символа обеих строк к нижнему регистру с помощью функции tolower
		return _stricmp(pStrA, pStrB);
	#else
		return strcasecmp(pStrA, pStrB);
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
int StrCaseCmp(const wchar_t* pStrA, const wchar_t* pStrB)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Нужна своя реализация: цикл сравнения с приведением
		// каждого символа обеих строк к нижнему регистру с помощью функции towlower
		return _wcsicmp(pStrA, pStrB);
	#else
		return wcscasecmp(pStrA, pStrB);
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
int StrNCmp(const char* pStrA, const char* pStrB, size_t count)
{
	// TODO: см. комментарий к StrCmp
	return strncmp(pStrA, pStrB, count);
}

//----------------------------------------------------------------------------------------------------------------------
int StrNCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count)
{
	// TODO: см. комментарий к StrCmp
	return wcsncmp(pStrA, pStrB, count);
}

//----------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const char* pStrA, const char* pStrB, size_t count)
{
	if (!count)
		return 0;

	int a, b;
	do {
		a = static_cast<unsigned char>(*pStrA++);
		a += loCaseTTA[a];
		b = static_cast<unsigned char>(*pStrB++);
		b += loCaseTTA[b];
	} while (b && a == b && --count);
	return a - b;
}

//----------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count)
{
	// См. комментарий к функции StrInsCmp
	static_assert(std::is_unsigned<wchar_t>::value || sizeof(wchar_t) == sizeof(unsigned),
		"Type wchar_t must be unsigned or same-sized as unsigned (int) type");

	if (!count)
		return 0;

	unsigned a, b;
	do {
		a = *pStrA++;
		if (a <= 0x7f)
			a += loCaseTTA[a];
		b = *pStrB++;
		if (b <= 0x7f)
			b += loCaseTTA[b];
	} while (--count && a && a == b);

	return (sizeof(wchar_t) < sizeof(a)) ? static_cast<int>(a - b) :
		(a == b) ? 0 : (a < b) ? -1 : 1;
}

//----------------------------------------------------------------------------------------------------------------------
int StrNCaseCmp(const char* pStrA, const char* pStrB, size_t count)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Нужна своя реализация: цикл сравнения с приведением
		// каждого символа обеих строк к нижнему регистру с помощью функции tolower
		return _strnicmp(pStrA, pStrB, count);
	#else
		return strncasecmp(pStrA, pStrB, count);
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
int StrNCaseCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Нужна своя реализация: цикл сравнения с приведением
		// каждого символа обеих строк к нижнему регистру с помощью функции towlower
		return _wcsnicmp(pStrA, pStrB, count);
	#else
		return wcsncasecmp(pStrA, pStrB, count);
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции преобразования регистра
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	#pragma warning(push)
	#pragma warning(disable: 4996)
#endif

//----------------------------------------------------------------------------------------------------------------------
static inline char* Strlwr(char* pStr, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает почти вдвое быстрее цикла с вызовом tolower в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _strlwr(pStr);
		#else
			for (char* p = pStr; *p; ++p)
			{
				unsigned char c = *p;
				*p = static_cast<char>(tolower(c));
			}
			return pStr;
		#endif
	}

	unsigned c;
	for (char* p = pStr;; ++p)
	{
		c = static_cast<unsigned char>(*p);
		if (c == 0)
			return pStr;

		c += loCaseTTA[c];
		*p = static_cast<char>(c);
	}
}

//----------------------------------------------------------------------------------------------------------------------
static inline char* Strupr(char* pStr, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает почти вдвое быстрее цикла с вызовом toupper в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _strupr(pStr);
		#else
			for (char* p = pStr; *p; ++p)
			{
				unsigned char c = *p;
				*p = static_cast<char>(toupper(c));
			}
			return pStr;
		#endif
	}

	unsigned c;
	for (char* p = pStr;; ++p)
	{
		c = static_cast<unsigned char>(*p);
		if (c == 0)
			return pStr;

		c -= upCaseTTA[c];
		*p = static_cast<char>(c);
	}
}

//----------------------------------------------------------------------------------------------------------------------
static inline wchar_t* Wcslwr(wchar_t* pStr, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает в ~4 раза быстрее цикла с вызовом towlower в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _wcslwr(pStr);
		#else
			for (wchar_t* p = pStr; *p; ++p)
				*p = static_cast<wchar_t>(towlower(*p));
			return pStr;
		#endif
	}

	for (wchar_t* p = pStr;; ++p)
	{
		unsigned c = *p;
		if (c == 0)
			return pStr;

		if (c <= 0x7f)
			c += loCaseTTA[c];
		*p = static_cast<wchar_t>(c);
	}
}

//----------------------------------------------------------------------------------------------------------------------
static inline wchar_t* Wcsupr(wchar_t* pStr, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает в ~4 раза быстрее цикла с вызовом towupper в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _wcsupr(pStr);
		#else
			for (wchar_t* p = pStr; *p; ++p)
				*p = static_cast<wchar_t>(towupper(*p));
			return pStr;
		#endif
	}

	for (wchar_t* p = pStr;; ++p)
	{
		unsigned c = *p;
		if (c == 0)
			return pStr;

		if (c <= 0x7f)
			c -= upCaseTTA[c];
		*p = static_cast<wchar_t>(c);
	}
}

#if AML_OS_WINDOWS
	#pragma warning(pop)
#endif

//----------------------------------------------------------------------------------------------------------------------
std::string LoCase(const std::string& str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::string();

	SmartArray<char, 512> buffer(len + 1, str.c_str());
	return std::string(Strlwr(buffer, !noLocale), len);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring LoCase(const std::wstring& str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::wstring();

	SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
	return std::wstring(Wcslwr(buffer, !noLocale), len);
}

//----------------------------------------------------------------------------------------------------------------------
std::string UpCase(const std::string& str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::string();

	SmartArray<char, 512> buffer(len + 1, str.c_str());
	return std::string(Strupr(buffer, !noLocale), len);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring UpCase(const std::wstring& str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::wstring();

	SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
	return std::wstring(Wcsupr(buffer, !noLocale), len);
}

//----------------------------------------------------------------------------------------------------------------------
void LoCaseInplace(std::string& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<char, 512> buffer(len + 1, str.c_str());
		str.assign(Strlwr(buffer, !noLocale), len);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void LoCaseInplace(std::wstring& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
		str.assign(Wcslwr(buffer, !noLocale), len);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void UpCaseInplace(std::string& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<char, 512> buffer(len + 1, str.c_str());
		str.assign(Strupr(buffer, !noLocale), len);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void UpCaseInplace(std::wstring& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
		str.assign(Wcsupr(buffer, !noLocale), len);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Trim
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
template<class StrType>
static size_t FindFirstNotSpace(const StrType& str)
{
	const size_t len = str.size();
	typename StrType::const_pointer p = str.c_str();
	for (size_t i = 0; i < len; ++i, ++p)
	{
		if (*p != 9 && *p != 32)
			return i;
	}
	return StrType::npos;
}

//----------------------------------------------------------------------------------------------------------------------
template<class StrType>
static size_t FindLastNotSpace(const StrType& str)
{
	const size_t len = str.size();
	typename StrType::const_pointer p = str.c_str() + len - 1;
	for (size_t i = len; i > 0; --i, --p)
	{
		if (*p != 9 && *p != 32)
			return i - 1;
	}
	return StrType::npos;
}

//----------------------------------------------------------------------------------------------------------------------
std::string Trim(const std::string& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == std::string::npos)
		return std::string();

	size_t endPos = FindLastNotSpace(str);
	return str.substr(startPos, endPos - startPos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring Trim(const std::wstring& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == std::wstring::npos)
		return std::wstring();

	size_t endPos = FindLastNotSpace(str);
	return str.substr(startPos, endPos - startPos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::string TrimLeft(const std::string& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == std::string::npos)
		return std::string();

	return str.substr(startPos);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring TrimLeft(const std::wstring& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == std::wstring::npos)
		return std::wstring();

	return str.substr(startPos);
}

//----------------------------------------------------------------------------------------------------------------------
std::string TrimRight(const std::string& str)
{
	size_t endPos = FindLastNotSpace(str);
	if (endPos == std::string::npos)
		return std::string();

	return str.substr(0, endPos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring TrimRight(const std::wstring& str)
{
	size_t endPos = FindLastNotSpace(str);
	if (endPos == std::wstring::npos)
		return std::wstring();

	return str.substr(0, endPos + 1);
}

//----------------------------------------------------------------------------------------------------------------------
template<class StrType>
static bool TrimStringInplace(StrType& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == StrType::npos)
	{
		str.clear();
		return true;
	}

	bool res = false;
	size_t endPos = FindLastNotSpace(str);
	if (endPos + 1 < str.size())
	{
		str.erase(endPos + 1);
		res = true;
	}
	if (startPos > 0)
	{
		str.erase(0, startPos);
		res = true;
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
void TrimInplace(std::string& str, bool fast)
{
	if (TrimStringInplace(str) && !fast)
		str.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------
void TrimInplace(std::wstring& str, bool fast)
{
	if (TrimStringInplace(str) && !fast)
		str.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------
void TrimLeftInplace(std::string& str, bool fast)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == 0)
		return;

	if (startPos != std::string::npos)
		str.erase(0, startPos);
	else
		str.clear();

	if (!fast)
		str.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------
void TrimLeftInplace(std::wstring& str, bool fast)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == 0)
		return;

	if (startPos != std::wstring::npos)
		str.erase(0, startPos);
	else
		str.clear();

	if (!fast)
		str.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------
void TrimRightInplace(std::string& str, bool fast)
{
	size_t endPos = FindLastNotSpace(str);

	if (endPos == std::string::npos)
		str.clear();
	else if (endPos + 1 < str.size())
		str.erase(endPos + 1);
	else
		return;

	if (!fast)
		str.shrink_to_fit();
}

//----------------------------------------------------------------------------------------------------------------------
void TrimRightInplace(std::wstring& str, bool fast)
{
	size_t endPos = FindLastNotSpace(str);

	if (endPos == std::wstring::npos)
		str.clear();
	else if (endPos + 1 < str.size())
		str.erase(endPos + 1);
	else
		return;

	if (!fast)
		str.shrink_to_fit();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк Ansi/Wide
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE std::wstring MBToWide(const char* pStr, size_t size)
{
	std::wstring res;

	#if AML_OS_WINDOWS
		if (size && size <= INT_MAX)
		{
			int len = static_cast<int>(size);
			int bufferLen = ::MultiByteToWideChar(CP_ACP, 0, pStr, len, nullptr, 0);
			if (bufferLen > 0)
			{
				SmartArray<wchar_t, 3840 / sizeof(wchar_t)> buffer(bufferLen);
				len = ::MultiByteToWideChar(CP_ACP, 0, pStr, len, buffer, bufferLen);
				if (len > 0)
					res.assign(buffer, len);
			}
		}
	#else
		#error Not implemented
	#endif

	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FromAnsi(const char* pStr)
{
	if (!pStr || !pStr[0])
		return std::wstring();
	return MBToWide(pStr, strlen(pStr));
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FromAnsi(const std::string& str)
{
	const size_t size = str.size();
	if (!size)
		return std::wstring();
	return MBToWide(str.c_str(), size);
}

//----------------------------------------------------------------------------------------------------------------------
int FromAnsi(wchar_t* pBuffer, size_t bufferSize, const char* pStr, int strLen)
{
	const size_t strSize = (strLen >= 0) ? strLen : pStr ? strlen(pStr) : 0;
	if (!strSize)
		return 0;

	#if AML_OS_WINDOWS
		if (strSize > INT_MAX)
			return -1;
		int size = pBuffer ? static_cast<int>((bufferSize < INT_MAX) ? bufferSize : INT_MAX) : 0;
		int len = ::MultiByteToWideChar(CP_ACP, 0, pStr, static_cast<int>(strSize), pBuffer, size);
		return (len > 0) ? len : -1;
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE std::string WideToMB(const wchar_t* pStr, size_t size)
{
	std::string res;

	#if AML_OS_WINDOWS
		if (size && size <= INT_MAX)
		{
			int len = static_cast<int>(size);
			int bufferLen = ::WideCharToMultiByte(CP_ACP, 0, pStr, len, nullptr, 0, nullptr, nullptr);
			if (bufferLen > 0)
			{
				SmartArray<char, 3840> buffer(bufferLen);
				len = ::WideCharToMultiByte(CP_ACP, 0, pStr, len, buffer, bufferLen, nullptr, nullptr);
				if (len > 0)
					res.assign(buffer, len);
			}
		}
	#else
		#error Not implemented
	#endif

	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::string ToAnsi(const wchar_t* pStr)
{
	if (!pStr || !pStr[0])
		return std::string();
	return WideToMB(pStr, wcslen(pStr));
}

//----------------------------------------------------------------------------------------------------------------------
std::string ToAnsi(const std::wstring& str)
{
	const size_t size = str.size();
	if (!size)
		return std::string();
	return WideToMB(str.c_str(), size);
}

//----------------------------------------------------------------------------------------------------------------------
int ToAnsi(char* pBuffer, size_t bufferSize, const wchar_t* pStr, int strLen)
{
	const size_t strSize = (strLen >= 0) ? strLen : pStr ? wcslen(pStr) : 0;
	if (!strSize)
		return 0;

	#if AML_OS_WINDOWS
		if (strSize > INT_MAX)
			return -1;
		int size = pBuffer ? static_cast<int>((bufferSize < INT_MAX) ? bufferSize : INT_MAX) : 0;
		int len = ::WideCharToMultiByte(CP_ACP, 0, pStr, static_cast<int>(strSize), pBuffer, size, nullptr, nullptr);
		return (len > 0) ? len : -1;
	#else
		#error Not implemented
	#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Форматирование строк
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && _MSC_VER < 1900
	// Начиная с версии MSVC 2015, функция vsnprintf полностью соответствует стандарту C99. Но в более
	// ранних версиях её поведение отличается. Функция vsnprintf_c99 исправляет поведение этой функции
	// на соответствующее стандарту ANSI C99. Также в версиях MSVC до 2015 отсутствует макрос va_copy
	#define AML_VSNPRINTF vsnprintf_c99
	#define va_copy(DST, SRC) DST = SRC

//----------------------------------------------------------------------------------------------------------------------
static int vsnprintf_c99(char* pBuffer, size_t bufferSize, const char* pFormat, va_list args)
{
	// Сохраняем указатель на буфер, чтобы потом записать в него терминирующий
	// "0". Указатель будет ненулевым, если буфер задан и его размер больше 0
	char* pTZBuffer = bufferSize ? pBuffer : nullptr;

	if (pBuffer)
	{
		// Буфер задан. Если размер буфера больше 0, то уменьшаем его
		// на 1, чтобы зарезервировать место для терминирующего "0"
		if (bufferSize)
			--bufferSize;
		// Если после этого размер стал равен 0 (то есть если он был 0 или 1), то обнуляем
		// и указатель на буфер, так как нам теперь нужно только подсчитать размер строки
		if (!bufferSize)
			pBuffer = nullptr;
	}

	va_list argsCopy;
	va_copy(argsCopy, args);

	#pragma warning(push)
	#pragma warning(disable: 4996)
	// В версиях MSVC до 2015 функции vsnprintf и _vsnprintf идентичны, и не соответствуют C99.
	// Так, в случае недостаточного размера буфера, функция вернёт -1 (а не длину результирующей
	// строки, какой бы она была, если бы буфер был достаточен). Также, терминирующий ноль будет
	// записан, только если в буфере будет достаточно для него места
	int len = _vsnprintf(pBuffer, bufferSize, pFormat, args);
	// Если len отрицательный, то это может означать, что произошла ошибка форматирования, или
	// что размер буфера недостаточен. Чтобы исключить второй вариант, вызываем функцию снова
	if (len < 0 && pBuffer)
		len = _vsnprintf(nullptr, 0, pFormat, argsCopy);
	// Пишем терминирующий ноль, если в буфере для него не хватило места
	if (pTZBuffer && len >= 0 && static_cast<size_t>(len) >= bufferSize)
		pTZBuffer[bufferSize] = 0;
	#pragma warning(pop)

	va_end(argsCopy);
	return len;
}

#else
	#define AML_VSNPRINTF vsnprintf
#endif // _MSC_VER

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE bool FormatStringA(size_t formattedLen, const char* pFormat,
	va_list args, void* pData, FormatFnA fmtFn)
{
	if (formattedLen < INT_MAX)
	{
		DynamicArray<char> buffer(formattedLen + 1);
		int len = vsnprintf(buffer, formattedLen + 1, pFormat, args);
		if (len >= 0 && static_cast<size_t>(len) <= formattedLen)
		{
			fmtFn(buffer, len, pData);
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE bool FormatStringW(const wchar_t* pFormat, va_list args, void* pData, FormatFnW fmtFn)
{
	// Обычно функции форматирования строк работают довольно медленно. Даже вычисление длины результирующей
	// строки без сохранения самой строки в буфер займёт значительно больше времени, чем выделение/освобождение
	// достаточно большого буфера в куче (даже если он сильно больше, чем необходимо). Эта функция вызывается,
	// когда форматирование в небольшой буфер на стеке завершилось с ошибкой. Не будем пытаться найти размер
	// результирующей строки, просто выделим достаточно большой буфер в куче. Но если форматируемая строка
	// и в самом деле огромна, то эта функция также завершится с ошибкой

	const size_t MAX_LENGTH = 8160;
	DynamicArray<wchar_t> buffer(MAX_LENGTH + 1);
	int len = vswprintf(buffer, MAX_LENGTH + 1, pFormat, args);
	if (len < 0)
		return false;

	fmtFn(buffer, len, pData);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::string Format(const char* pFormat, ...)
{
	std::string res;
	if (pFormat && *pFormat)
	{
		const size_t LOCAL_SIZE = 3840;
		char localBuffer[LOCAL_SIZE];

		va_list args;
		va_start(args, pFormat);
		// Сначала попытаемся сразу сформатировать строку в небольшой буфер localBuffer,
		// выделенный на стеке. В случае успеха, т.е. если результирующая строка поместится
		// в этом буфере, мы ограничимся всего одним вызовом функции *printf
		int len = AML_VSNPRINTF(localBuffer, LOCAL_SIZE, pFormat, args);
		va_end(args);

		if (len >= LOCAL_SIZE)
		{
			va_start(args, pFormat);
			// Нам не удалось сформатировать строку из-за того, что буфер оказался
			// слишком мал. Пробуем другой способ с выделением буфера в куче
			FormatStringA(len, pFormat, args, &res, [](const char* pStr, size_t strLen, void* pData) {
				std::string* pResult = reinterpret_cast<std::string*>(pData);
				if (strLen)
					pResult->assign(pStr, strLen);
			});
			va_end(args);
		}
		else if (len > 0)
			res.assign(localBuffer, len);
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::wstring Format(const wchar_t* pFormat, ...)
{
	std::wstring res;
	if (pFormat && *pFormat)
	{
		const size_t LOCAL_SIZE = 3840 / sizeof(wchar_t);
		wchar_t localBuffer[LOCAL_SIZE];

		va_list args;
		va_start(args, pFormat);
		// Сначала попытаемся сразу сформатировать строку в небольшой буфер localBuffer,
		// выделенный на стеке. В случае успеха, т.е. если результирующая строка поместится
		// в этом буфере, мы ограничимся всего одним вызовом функции *printf
		int len = vswprintf(localBuffer, LOCAL_SIZE, pFormat, args);
		va_end(args);

		if (len < 0)
		{
			va_start(args, pFormat);
			// Нам не удалось сформатировать строку. Возможно из-за того, что буфер
			// оказался слишком мал. Пробуем другой способ с выделением буфера в куче
			FormatStringW(pFormat, args, &res, [](const wchar_t* pStr, size_t strLen, void* pData) {
				std::wstring* pResult = reinterpret_cast<std::wstring*>(pData);
				if (strLen)
					pResult->assign(pStr, strLen);
			});
			va_end(args);
		}
		else if (len > 0)
			res.assign(localBuffer, len);
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FormatW(const char* pFormat, ...)
{
	std::wstring res;
	if (pFormat && *pFormat)
	{
		va_list args;
		va_start(args, pFormat);
		FormatEx(pFormat, args, &res, [](const char* pStr, size_t strLen, void* pData) {
			std::wstring* pResult = reinterpret_cast<std::wstring*>(pData);
			if (strLen)
				*pResult = MBToWide(pStr, strLen);
		});
		va_end(args);
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FormatEx(const char* pFormat, va_list args, void* pData, FormatFnA fmtFn)
{
	if (!fmtFn)
		return false;

	const size_t LOCAL_SIZE = 3840;
	char localBuffer[LOCAL_SIZE];
	localBuffer[0] = 0;
	int len = 0;

	if (pFormat && *pFormat)
	{
		va_list argsCopy;
		va_copy(argsCopy, args);
		len = AML_VSNPRINTF(localBuffer, LOCAL_SIZE, pFormat, argsCopy);
		va_end(argsCopy);

		if (len >= LOCAL_SIZE)
			return FormatStringA(len, pFormat, args, pData, fmtFn);
		else if (len < 0)
			return false;
	}

	fmtFn(localBuffer, len, pData);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FormatEx(const wchar_t* pFormat, va_list args, void* pData, FormatFnW fmtFn)
{
	if (!fmtFn)
		return false;

	const size_t LOCAL_SIZE = 3840 / sizeof(wchar_t);
	wchar_t localBuffer[LOCAL_SIZE];
	localBuffer[0] = 0;
	int len = 0;

	if (pFormat && *pFormat)
	{
		va_list argsCopy;
		va_copy(argsCopy, args);
		len = vswprintf(localBuffer, LOCAL_SIZE, pFormat, argsCopy);
		va_end(argsCopy);

		if (len < 0)
			return FormatStringW(pFormat, args, pData, fmtFn);
	}

	fmtFn(localBuffer, len, pData);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция Split
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
template<class StrType>
static size_t FindFirstOf(const StrType& str, typename StrType::const_pointer pWhat, size_t from)
{
	if (pWhat && *pWhat)
	{
		const size_t len = str.size();
		typename StrType::const_pointer p = str.c_str();

		if (pWhat[1] == 0)
		{
			for (size_t i = from; i < len; ++i)
			{
				if (p[i] == *pWhat)
					return i;
			}
		} else
		{
			for (size_t i = from; i < len; ++i)
			{
				for (size_t j = 0; pWhat[j]; ++j)
				{
					if (p[i] == pWhat[j])
						return i;
				}
			}
		}
	}
	return StrType::npos;
}

//----------------------------------------------------------------------------------------------------------------------
template<class StrType>
static void Split(std::vector<StrType>& tokens, const StrType& str,
	typename StrType::const_pointer pDelimiters, int flags)
{
	const size_t MAX_DELIM_C = 32;
	size_t delimPosA[MAX_DELIM_C];

	size_t tokenC = 0, lastDelim = StrType::npos;
	for (size_t fromPos = 0, delimC = 0; fromPos < str.size();)
	{
		lastDelim = FindFirstOf(str, pDelimiters, fromPos);
		if (delimC < MAX_DELIM_C)
			delimPosA[delimC++] = lastDelim;
		if (lastDelim == fromPos)
		{
			++fromPos;
			if (flags & SPLIT_ALLOW_EMPTY)
				++tokenC;
		} else
		{
			fromPos = (lastDelim == StrType::npos) ? str.size() : lastDelim + 1;
			++tokenC;
		}
	}
	const int TRAILING_FLAG = SPLIT_ALLOW_EMPTY | SPLIT_TRAILING_DELIMITER;
	if ((flags & TRAILING_FLAG) == TRAILING_FLAG && lastDelim != StrType::npos)
		++tokenC;
	else if (!tokenC)
		return;

	tokens.reserve(tokenC);
	for (size_t fromPos = 0, delimIdx = 0; fromPos < str.size();)
	{
		lastDelim = (delimIdx < MAX_DELIM_C) ? delimPosA[delimIdx++] :
			FindFirstOf(str, pDelimiters, fromPos);
		if (lastDelim == fromPos)
		{
			++fromPos;
			if (flags & SPLIT_ALLOW_EMPTY)
				tokens.emplace_back();
		} else
		{
			size_t tokenLen = ((lastDelim == StrType::npos) ? str.size() : lastDelim) - fromPos;
			typename StrType::const_pointer pLeft = str.c_str() + fromPos;
			fromPos += tokenLen + 1;

			if (flags & SPLIT_TRIM)
			{
				typename StrType::const_pointer pRight = pLeft + tokenLen - 1;
				while (pLeft <= pRight && (*pLeft == 32 || *pLeft == 9)) ++pLeft;
				if (pLeft < pRight) while (*pRight == 32 || *pRight == 9) --pRight;
				tokenLen = (pLeft <= pRight) ? pRight - pLeft + 1 : 0;
			}
			if (tokenLen || (flags & SPLIT_ALLOW_EMPTY))
				tokens.emplace_back(pLeft, tokenLen);
		}
	}
	if ((flags & TRAILING_FLAG) == TRAILING_FLAG && lastDelim != StrType::npos)
		tokens.emplace_back();
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<std::string> Split(const std::string& str, const char* pDelimiters, int flags)
{
	std::vector<std::string> tokens;
	Split(tokens, str, pDelimiters, flags);
	return tokens;
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<std::wstring> Split(const std::wstring& str, const wchar_t* pDelimiters, int flags)
{
	std::vector<std::wstring> tokens;
	Split(tokens, str, pDelimiters, flags);
	return tokens;
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<std::string> Split(const std::string& str, const std::string& delimiters, int flags)
{
	return Split(str, delimiters.c_str(), flags);
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<std::wstring> Split(const std::wstring& str, const std::wstring& delimiters, int flags)
{
	return Split(str, delimiters.c_str(), flags);
}

} // namespace util
