#include "strutil.h"

#include "../../core/array.h"
#include "../../core/winapi.h"
#include "utfutil.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Strlen и Wcslen (своя реализация функций CRT)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !AML_USEASM

//----------------------------------------------------------------------------------------------------------------------
#define STRLEN_RET_LE(VALUE, SHIFT) \
	if ((VALUE & 0xff) == 0) return p - pStr - sizeof(size_t) + SHIFT;			\
	if ((VALUE & 0xff00) == 0) return p - pStr - sizeof(size_t) + SHIFT + 1;	\
	if ((VALUE & 0xff0000) == 0) return p - pStr - sizeof(size_t) + SHIFT + 2;	\
	if ((VALUE & 0xff000000) == 0) return p - pStr - sizeof(size_t) + SHIFT + 3;

//----------------------------------------------------------------------------------------------------------------------
#define STRLEN_RET_BE(VALUE, SHIFT) \
	if ((VALUE & 0xff000000) == 0) return p - pStr - sizeof(size_t) + SHIFT;	\
	if ((VALUE & 0xff0000) == 0) return p - pStr - sizeof(size_t) + SHIFT + 1;	\
	if ((VALUE & 0xff00) == 0) return p - pStr - sizeof(size_t) + SHIFT + 2;	\
	if ((VALUE & 0xff) == 0) return p - pStr - sizeof(size_t) + SHIFT + 3;

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE size_t AML_FASTCALL Strlen(const char* pStr)
{
	const size_t MAGIC = (sizeof(size_t) == 4) ? 0x7efefeff : 0x7efefefefefefeff;
	const size_t MASK  = (sizeof(size_t) == 4) ? 0x81010100 : 0x8101010101010100;

	const char* p = pStr;
	// Выравниваем указатель p на границу 4 (x86) или 8 (x64) байт. В основном это
	// нужно, чтобы не выйти за границы страницы памяти при чтении слов в цикле.
	for (; reinterpret_cast<uintptr_t>(p) & (sizeof(size_t) - 1); ++p)
		if (*p == 0) return p - pStr;
	// Основной цикл: обрабатываем по 2x4 или 2x8 байт за итерацию.
	for (const size_t* pLong = reinterpret_cast<const size_t*>(p);;)
	{
		size_t v = *pLong++;
		if ((((v + MAGIC) ^ ~v) & MASK) == 0)
		{
			v = *pLong++;
			if ((((v + MAGIC) ^ ~v) & MASK) == 0)
				continue;
		}
		v = *(pLong - 1);
		p = reinterpret_cast<const char*>(pLong);

#if AML_LITTLE_ENDIAN
		STRLEN_RET_LE(v, 0)
		if (sizeof(v) != 4)
		{
			v >>= 32;
			STRLEN_RET_LE(v, 4)
		}
#else
		if (sizeof(v) == 4)
		{
			STRLEN_RET_BE(v, 0)
		} else
		{
			size_t h = v >> 32;
			STRLEN_RET_BE(h, 0)
			STRLEN_RET_BE(v, 4)
		}
#endif
	}
}

//----------------------------------------------------------------------------------------------------------------------
#define WCSLEN(P) \
	((uintptr_t(pLong) - uintptr_t(pStr)) / sizeof(wchar_t) - sizeof(size_t) / sizeof(wchar_t) + P)

//----------------------------------------------------------------------------------------------------------------------
#define WCSLEN_RET_LE(VALUE, SHIFT) \
	if ((VALUE & 0xffff) == 0) return WCSLEN(SHIFT + 0);	\
	if ((VALUE & 0xffff0000) == 0) return WCSLEN(SHIFT + 1);

//----------------------------------------------------------------------------------------------------------------------
#define WCSLEN_RET_BE(VALUE, SHIFT) \
	if ((VALUE & 0xffff0000) == 0) return WCSLEN(SHIFT + 0);	\
	if ((VALUE & 0xffff) == 0) return WCSLEN(SHIFT + 1);

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE size_t AML_FASTCALL Wcslen(const wchar_t* pStr)
{
	const wchar_t* p = pStr;
	if (sizeof(wchar_t) == 2)
	{
		const size_t MAGIC = (sizeof(size_t) == 4) ? 0x7ffeffff : 0x7ffefffefffeffff;
		const size_t MASK  = (sizeof(size_t) == 4) ? 0x80010000 : 0x8001000100010000;
		// Выравниваем указатель p на границу 4(-1) (x86) или 8(-1) (x64) байт. В основном
		// это нужно, чтобы не выйти за границы страницы памяти при чтении слов в цикле.
		for (; (reinterpret_cast<uintptr_t>(p) + 1) & (sizeof(size_t) - 2); ++p)
			if (*p == 0) return p - pStr;
		// Основной цикл: обрабатываем по 2x2 или 2x4 символа за итерацию.
		for (const size_t* pLong = reinterpret_cast<const size_t*>(p);;)
		{
			size_t v = *pLong++;
			if ((((v + MAGIC) ^ ~v) & MASK) == 0)
			{
				v = *pLong++;
				if ((((v + MAGIC) ^ ~v) & MASK) == 0)
					continue;
			}
			v = *(pLong - 1);

#if AML_LITTLE_ENDIAN
			WCSLEN_RET_LE(v, 0)
			if (sizeof(v) != 4)
			{
				v >>= 32;
				WCSLEN_RET_LE(v, 2)
			}
#else
			if (sizeof(v) == 4)
			{
				WCSLEN_RET_BE(v, 0)
			} else
			{
				size_t h = v >> 32;
				WCSLEN_RET_BE(h, 0)
				WCSLEN_RET_BE(v, 2)
			}
#endif
		}
	}
	else if ((sizeof(wchar_t) == 4) && (sizeof(size_t) == 8))
	{
		const uint64_t MAGIC = 0x7ffffffeffffffff;
		const uint64_t MASK  = 0x8000000100000000;
		// Выравниваем указатель p на границу 8(-3) байт. Это нужно, чтобы
		// не выйти за границы страницы памяти при чтении слов в цикле.
		for (; (reinterpret_cast<uintptr_t>(p) + 3) & (8 - 4); ++p)
			if (*p == 0) return p - pStr;
		// Основной цикл: обрабатываем по 2x2 или 2x4 символа за итерацию.
		for (const size_t* pLong = reinterpret_cast<const size_t*>(p);;)
		{
			uint64_t v = *pLong++;
			if ((((v + MAGIC) ^ ~v) & MASK) == 0)
			{
				v = *pLong++;
				if ((((v + MAGIC) ^ ~v) & MASK) == 0)
					continue;
			}
			v = *(pLong - 1);

#if AML_LITTLE_ENDIAN
			if ((v & 0xffffffff) == 0) return WCSLEN(0);
			if ((v >> 32) == 0) return WCSLEN(1);
#else
			if ((v >> 32) == 0) return WCSLEN(0);
			if ((v & 0xffffffff) == 0) return WCSLEN(1);
#endif
		}
	} else
	{
		while (*p++);
		return p - pStr - 1;
	}
}

#endif  // AML_USEASM

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк Ansi/Wide
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static std::wstring FromUTF8(const char* pStr, size_t size)
{
	const size_t LOCAL_SIZE = 960;
	wchar_t localBuffer[LOCAL_SIZE];

	size_t (*cvtFn)(void*, size_t, const char*, size_t);
	cvtFn = (sizeof(wchar_t) == 2) ? UTF8To16 : UTF8To32;

	std::wstring res;
	if (size_t len = cvtFn(localBuffer, LOCAL_SIZE, pStr, size))
	{
		if (len <= LOCAL_SIZE)
			res.assign(localBuffer, len);
		else
		{
			const size_t bufferSize = len;
			DynamicArray<wchar_t> buffer(bufferSize);
			len = cvtFn(buffer, bufferSize, pStr, size);
			if (len && (len <= bufferSize))
				res.assign(buffer, len);
		}
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FromUTF8(const char* pStr)
{
	if (!pStr) return std::wstring();
	return FromUTF8(pStr, Strlen(pStr));
}

//----------------------------------------------------------------------------------------------------------------------
std::wstring FromUTF8(const std::string& str)
{
	const size_t size = str.size();
	if (size == 0) return std::wstring();
	return FromUTF8(str.c_str(), size);
}

//----------------------------------------------------------------------------------------------------------------------
static std::string ToUTF8(const wchar_t* pStr, size_t size)
{
	const size_t LOCAL_SIZE = 960;
	char localBuffer[LOCAL_SIZE];

	size_t (*cvtFn)(char*, size_t, const void*, size_t) = UTF16To8;
	if (sizeof(wchar_t) != 2) cvtFn = UTF32To8;

	std::string res;
	if (size_t len = cvtFn(localBuffer, LOCAL_SIZE, pStr, size))
	{
		if (len <= LOCAL_SIZE)
			res.assign(localBuffer, len);
		else
		{
			const size_t bufferSize = len;
			DynamicArray<char> buffer(bufferSize);
			len = cvtFn(buffer, bufferSize, pStr, size);
			if (len && (len <= bufferSize))
				res.assign(buffer, len);
		}
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
std::string ToUTF8(const wchar_t* pStr)
{
	if (!pStr) return std::string();
	return ToUTF8(pStr, Wcslen(pStr));
}

//----------------------------------------------------------------------------------------------------------------------
std::string ToUTF8(const std::wstring& str)
{
	const size_t size = str.size();
	if (size == 0) return std::string();
	return ToUTF8(str.c_str(), size);
}

} // namespace util
