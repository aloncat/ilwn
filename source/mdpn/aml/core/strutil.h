//∙AML
// Copyright (C) 2017-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "../../../aml/core/strutil.h"

#include <core/platform.h>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Strlen и Wcslen (оптимизированные аналоги функций CRT)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t AML_FASTCALL Strlen(const char* str) noexcept;
size_t AML_FASTCALL Wcslen(const wchar_t* str) noexcept;

inline size_t GetLength(const char* str) noexcept { return Strlen(str); }
inline size_t GetLength(const wchar_t* str) noexcept { return Wcslen(str); }

} // namespace util
