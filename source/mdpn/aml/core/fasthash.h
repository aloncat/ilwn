//⬪AML⬪
#pragma once

#include "platform.h"

#include <string>

namespace hash {

// Функции GetFastHash вычисляют 32-битный хеш (FNV-1a) от заданной строки. Функции с парметром toLower,
// если его значение равно true, ведут себя так, как если бы каждый символ исходной строки был переведён
// в нижний регистр перед вычислением хеша (применимо только к латинским буквам от 'A' до 'Z')

// Возвращает значение хеша для пустой строки (зерно FNV-1a)
constexpr unsigned GetFastHash() { return /*FNV_SEED*/ 0x811c9dc5; }

// Вычисляет хеш null-terminated строки pStr, состоящей из 8-битных символов.
// Функция может быть использована как для Ansi, так и для UTF-8 строк
unsigned GetFastHash(const char* pStr, bool toLower = false);

// Вычисляет хеш null-terminated Wide строки pStr
unsigned GetFastHash(const wchar_t* pStr, bool toLower = false);

// Вычисляет хеш null-terminated строки pStr, состоящей из 16-битных символов. Функция подходит
// для UTF-16 строк. Эта функция даёт одинаковый хеш на little-endian и big-endian платформах
unsigned GetFastHash(const uint16_t* pStr);

// Вычисляет хеш для count первых символов строки pStr, которая состоит из 16-битных
// символов. Функция даёт одинаковый хеш на little-endian и big-endian платформах
unsigned GetFastHash(const uint16_t* pStr, size_t count);

// Вычисляет хеш массива pData длиной size байт. Параметр prevHash может
// использоваться для инкрементного вычисления хеша или задания значения зерна
unsigned GetFastHash(const void* pData, size_t size, unsigned prevHash = GetFastHash());

//----------------------------------------------------------------------------------------------------------------------
inline unsigned GetFastHash(const std::string& str, bool toLower = false)
{
	return GetFastHash(str.c_str(), toLower);
}

//----------------------------------------------------------------------------------------------------------------------
inline unsigned GetFastHash(const std::wstring& str, bool toLower = false)
{
	return GetFastHash(str.c_str(), toLower);
}

} // namespace hash
