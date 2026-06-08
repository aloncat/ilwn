//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

// Возвращает строку, содержащую версию программы: дату и время в формате "YYYY-MM-DD HH:MM". Дата и время
// берётся из макросов __DATE__ и __TIME__ соответственно. В отладочных сборках возвращает "[DEBUG]"
std::string GetAppVersion();

// Возвращает строку, содержащую дату и время в формате "YYYY-MM-DD HH:MM". Параметры date и time должны
// задавать дату и время в формате, идентичном строкам макросов __DATE__ и __TIME__ соответственно.
// Если параметр time не задан, то результирующая строка будет содержать только дату
std::string GetBuildDateTime(const char* date, const char* time = nullptr);
