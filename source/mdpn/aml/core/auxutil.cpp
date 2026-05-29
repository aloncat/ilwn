//⬪AML⬪
#include "pch.h"
#include "auxutil.h"

#include "array.h"
#include "console.h"
#include "platform.h"
#include "strutil.h"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace aux {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Print
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Print(const char* pStr, int color)
{
	util::SystemConsole::Instance().Write(pStr, color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Print(const wchar_t* pStr, int color)
{
	util::SystemConsole::Instance().Write(pStr, color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Print(const std::string& str, int color)
{
	util::SystemConsole::Instance().Write(str, color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Print(const std::wstring& str, int color)
{
	util::SystemConsole::Instance().Write(str, color);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Printc и Printf
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
template<class CharType>
static void PrintColored(const CharType* pStr, size_t strLen, void* = nullptr)
{
	CharType localBuffer[640];
	util::FlexibleArray<CharType> buffer(localBuffer);
	CharType* pOut = buffer;

	int color, nextColor = 7;
	const CharType* const pEnd = pStr + strLen;
	for (const CharType* p = pStr; p < pEnd;)
	{
		color = nextColor;
		CharType* const pOutEnd = std::min(buffer + buffer.GetSize() - 1, pOut + (pEnd - p));

		while (pOut < pOutEnd && *p != '#')
			*pOut++ = *p++;

		if (pOut < pOutEnd)
		{
			size_t i = 0;
			nextColor = 0;
			for (++p; i < 3 && p < pEnd && *p >= '0' && *p <= '9'; ++i, ++p)
				nextColor = 10 * nextColor + *p - '0';
			if (p < pEnd && *p == '#')
				++p;
			if (i == 0)
			{
				nextColor = color;
				*pOut++ = '#';
				if (p < pEnd)
					continue;
			}
			else if (nextColor == color && p < pEnd)
				continue;
		}
		else if (p < pEnd && buffer.GetSize() < 10240)
		{
			const size_t writtenC = pOut - buffer;
			buffer.Grow(2 * buffer.GetSize(), true);
			pOut = buffer + writtenC;
			continue;
		}

		if (pOut > buffer)
		{
			*pOut = 0;
			util::SystemConsole::Instance().Write(buffer, color);
			pOut = buffer;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printc(const char* pColoredStr)
{
	if (pColoredStr)
		PrintColored(pColoredStr, strlen(pColoredStr));
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printc(const wchar_t* pColoredStr)
{
	if (pColoredStr)
		PrintColored(pColoredStr, wcslen(pColoredStr));
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printc(const std::string& coloredStr)
{
	PrintColored(coloredStr.c_str(), coloredStr.size());
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printc(const std::wstring& coloredStr)
{
	PrintColored(coloredStr.c_str(), coloredStr.size());
}

//----------------------------------------------------------------------------------------------------------------------
template<class CharType>
static void PrintfHelper(const CharType* pFormat, va_list args)
{
	if (pFormat && *pFormat)
	{
		// Сканируем строку, чтобы проверить, нуждается ли она в форматировании. Если в строке содержатся только
		// коды цвета, то прямой вызов PrintColored даст значительное ускорение. В противном случае, проверка не
		// повлияет (если '%' расположен в начале строки), или увеличит время выполнения не более, чем на 1%
		const CharType* p = pFormat;
		while (*p && *p != '%') ++p;

		if (*p == 0)
			PrintColored(pFormat, p - pFormat);
		else
			util::FormatEx(pFormat, args, nullptr, PrintColored);
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printf(const char* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	PrintfHelper(pFormat, args);
	va_end(args);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Printf(const wchar_t* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	PrintfHelper(pFormat, args);
	va_end(args);
}

} // namespace aux
