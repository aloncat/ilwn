#pragma once

#include "../../../../aml/core/platform.h"
#include "../../../../aml/core/strutil.h"

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

} // namespace util
