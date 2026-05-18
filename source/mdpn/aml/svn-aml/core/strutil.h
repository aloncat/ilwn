#pragma once

#include "../../core/platform.h"
#include "../../core/strutil.h"

#include <string>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Strlen и Wcslen (своя реализация функций CRT)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {
	size_t AML_FASTCALL Strlen(const char* pStr);
	size_t AML_FASTCALL Wcslen(const wchar_t* pStr);
}

inline size_t GetLength(const char* pStr) { return Strlen(pStr); }
inline size_t GetLength(const wchar_t* pStr) { return Wcslen(pStr); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк UTF8/Wide
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Эти функции преобразуют UTF-8 строку в Wide строку.
std::wstring FromUTF8(const char* pStr);
std::wstring FromUTF8(const std::string& str);

// Эти функции преобразуют Wide строку в строку UTF-8.
std::string ToUTF8(const wchar_t* pStr);
std::string ToUTF8(const std::wstring& str);

} // namespace util
