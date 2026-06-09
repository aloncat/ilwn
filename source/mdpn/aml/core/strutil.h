//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "../../../aml/core/strutil.h"

#include <core/platform.h>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Strlen и Wcslen (своя реализация функций CRT)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t AML_FASTCALL Strlen(const char* pStr);
size_t AML_FASTCALL Wcslen(const wchar_t* pStr);

inline size_t GetLength(const char* pStr) { return Strlen(pStr); }
inline size_t GetLength(const wchar_t* pStr) { return Wcslen(pStr); }

} // namespace util
