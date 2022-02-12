//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/platform.h>

#include <string>
#include <string_view>

// Форматирует число, разделяя цифры символом separator на группы по 3
std::string SeparateWithCommas(uint64_t number, char separator = ',');

// Возвращает строку, символы которой разделены на группы по три
std::string SeparateWithCommas(std::string_view str, char separator = ',');
