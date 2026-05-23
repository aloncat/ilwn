//⬪AML⬪
#pragma once

#include <stdarg.h>
#include <string>
#include <vector>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные макросы (сравнение строк)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define AML_STRCMP_OVERLOADS(FN, S) \
	inline int FN(const S& strA, S::const_pointer pStrB) { return FN(strA.c_str(), pStrB); } \
	inline int FN(S::const_pointer pStrA, const S& strB) { return FN(pStrA, strB.c_str()); } \
	inline int FN(const S& strA, const S& strB) { return FN(strA.c_str(), strB.c_str()); }

#define AML_STRNCMP_OVERLOADS(FN, S) \
	inline int FN(const S& strA, S::const_pointer pStrB, size_t count) { return FN(strA.c_str(), pStrB, count); } \
	inline int FN(S::const_pointer pStrA, const S& strB, size_t count) { return FN(pStrA, strB.c_str(), count); } \
	inline int FN(const S& strA, const S& strB, size_t count) { return FN(strA.c_str(), strB.c_str(), count); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrCmp идентична стандартным функциям strcmp/wcscmp, лексикографически сравнивает строку "A" со
// строкой "B". Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт
// отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrCmp(const char* pStrA, const char* pStrB);
int StrCmp(const wchar_t* pStrA, const wchar_t* pStrB);

AML_STRCMP_OVERLOADS(StrCmp, std::string)
AML_STRCMP_OVERLOADS(StrCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrInsCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrInsCmp похожа на POSIX-функции strcasecmp/wcscasecmp, лексикографически сравнивает строку "A" со
// строкой "B" без учёта регистра, но работает так, как если бы использовалась "C" локаль, т.е. регистр символов
// игнорируется только для латинских букв 'a'..'z' и 'A'..'Z', а для остальных символов поведение этой функции
// идентично StrCmp. Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт
// отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrInsCmp(const char* pStrA, const char* pStrB);
int StrInsCmp(const wchar_t* pStrA, const wchar_t* pStrB);

AML_STRCMP_OVERLOADS(StrInsCmp, std::string)
AML_STRCMP_OVERLOADS(StrInsCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrCaseCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrCaseCmp идентична POSIX-функциям strcasecmp/wcscasecmp, лексикографически сравнивает строку "A"
// со строкой "B" без учёта регистра, используя текущую локаль. Для этого соответствующие символы обеих строк
// сначала приводятся к нижнему регистру с помощью функции tolower/towlower, после чего символы сравниваются.
// Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное
// число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrCaseCmp(const char* pStrA, const char* pStrB);
int StrCaseCmp(const wchar_t* pStrA, const wchar_t* pStrB);

AML_STRCMP_OVERLOADS(StrCaseCmp, std::string)
AML_STRCMP_OVERLOADS(StrCaseCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNCmp идентична стандартным функциям strncmp/wcsncmp, лексикографически сравнивает строку "A" со
// строкой "B", сравнивая не более count их первых символов. Т.е. если длина строки "A" или строки "B" больше
// count, то функция считает, что соответствующая строка содержит только count её первых символов. Если
// строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное
// число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrNCmp(const char* pStrA, const char* pStrB, size_t count);
int StrNCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count);

AML_STRNCMP_OVERLOADS(StrNCmp, std::string)
AML_STRNCMP_OVERLOADS(StrNCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNInsCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNInsCmp идентична функции StrInsCmp с той лишь разницей, что сравнивает не более count первых
// символов указанных строк. Функция сравнивает строки лексикографически без учёта регистра, работая так, как
// если бы использовалась "C" локаль. Если строки одинаковы (первые count символов строк в случае, когда одна
// или обе строки длиннее count символов), функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт
// отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrNInsCmp(const char* pStrA, const char* pStrB, size_t count);
int StrNInsCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count);

AML_STRNCMP_OVERLOADS(StrNInsCmp, std::string)
AML_STRNCMP_OVERLOADS(StrNInsCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNCaseCmp
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNCaseCmp идентична функции StrCaseCmp с той лишь разницей, что сравнивает не более count первых
// символов указанных строк. Функция сравнивает строки лексикографически без учёта регистра, используя текущую
// локаль. Если строки одинаковы (первые count символов строк в случае, когда одна или обе строки длиннее
// count символов), функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное
// число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrNCaseCmp(const char* pStrA, const char* pStrB, size_t count);
int StrNCaseCmp(const wchar_t* pStrA, const wchar_t* pStrB, size_t count);

AML_STRNCMP_OVERLOADS(StrNCaseCmp, std::string)
AML_STRNCMP_OVERLOADS(StrNCaseCmp, std::wstring)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции преобразования регистра
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функции LoCase и UpCase переводят все символы строки str в нижний (LoCase) или верхний (UpCase) регистр. Функции
// используют текущую локаль программы, если noLocale == false. Если параметр noLocale == true, то функции работают
// так, как если бы использовалась "C" локаль, т.е. регистр символов изменяется только для латинских букв 'a'..'z'
// (для UpCase) и 'A'..'Z' (для LoCase), а остальные символы не меняются. ПРИМЕЧАНИЕ: эти функции всегда выполняют
// преобразование 1:1, т.е. например, из немецкой ß (нижний регистр) нельзя получить SS (верхний)
std::string LoCase(const std::string& str, bool noLocale = false);
std::wstring LoCase(const std::wstring& str, bool noLocale = false);
std::string UpCase(const std::string& str, bool noLocale = false);
std::wstring UpCase(const std::wstring& str, bool noLocale = false);

// Функции *CaseInplace аналогичны функциям LoCase/UpCase, но изменяют непосредственно строку str, не
// возвращая результат. ПРИМЕЧАНИЕ: функции всегда выполняют преобразование 1:1 (см. примечание выше)
void LoCaseInplace(std::string& str, bool noLocale = false);
void LoCaseInplace(std::wstring& str, bool noLocale = false);
void UpCaseInplace(std::string& str, bool noLocale = false);
void UpCaseInplace(std::wstring& str, bool noLocale = false);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Trim
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция Trim удаляет из строки str начальные и конечные пробелы и символы
// табуляции. TrimLeft удаляет только начальные, а TrimRight - только конечные
std::string Trim(const std::string& str);
std::wstring Trim(const std::wstring& str);
std::string TrimLeft(const std::string& str);
std::wstring TrimLeft(const std::wstring& str);
std::string TrimRight(const std::string& str);
std::wstring TrimRight(const std::wstring& str);

// Функции Trim*Inplace отличаются от обычных функций Trim* тем, что изменяют непосредственно строку str
// и не возвращают результат. Параметр fast позволяет выбрать способ модификации строки: если он равен
// true, то будет использован быстрый метод без изменения размера занимаемой строкой памяти; если
// параметр fast равен false, то по возможности объём пямяти, занимаемый строкой, будет уменьшен
void TrimInplace(std::string& str, bool fast = true);
void TrimInplace(std::wstring& str, bool fast = true);
void TrimLeftInplace(std::string& str, bool fast = true);
void TrimLeftInplace(std::wstring& str, bool fast = true);
void TrimRightInplace(std::string& str, bool fast = true);
void TrimRightInplace(std::wstring& str, bool fast = true);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк Ansi/Wide
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Преобразует Ansi строку в строку Unicode
// (Wide), используя текущую системную локаль
std::wstring FromAnsi(const char* pStr);
std::wstring FromAnsi(const std::string& str);

// Преобразует Ansi строку pStr в строку Unicode (Wide) и сохраняет её в pBuffer. Параметр strLen задаёт
// количество обрабатываемых символов исходной строки; он может быть равен -1 для null-terminated строки, в
// этом случае длина строки будет вычислена автоматически. Параметр bufferSize задаёт размер буфера (в символах).
// Если размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов. В
// случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если pBuffer или bufferSize равен 0,
// то функция вернёт необходимый размер буфера (в символах). Функция не добавляет в pBuffer терминирующий 0
int FromAnsi(wchar_t* pBuffer, size_t bufferSize, const char* pStr, int strLen = -1);

// Преобразует Unicode (Wide) строку в строку
// Ansi, используя текущую системную локаль
std::string ToAnsi(const wchar_t* pStr);
std::string ToAnsi(const std::wstring& str);

// Преобразует Unicode (Wide) строку pStr в строку Ansi и сохраняет её в pBuffer. Параметр strLen задаёт
// количество обрабатываемых символов исходной строки; он может быть равен -1 для null-terminated строки,
// в этом случае длина строки будет вычислена автоматически. Параметр bufferSize задаёт размер буфера. Если
// размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов. В
// случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если pBuffer или bufferSize равен
// 0, то функция вернёт необходимый размер буфера. Функция не добавляет в pBuffer терминирующий 0
int ToAnsi(char* pBuffer, size_t bufferSize, const wchar_t* pStr, int strLen = -1);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Форматирование строк
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция Format возвращает форматированную строку. Спецификаторы формата для pFormat полностью идентичны
// спецификаторам функции printf. ПРИМЕЧАНИЕ: Wide-версия функции имеет ограничение длины результирующей
// строки: если она окажется длиннее 8160 символов, то функция вернёт пустую строку
std::string Format(const char* pFormat, ...);
std::wstring Format(const wchar_t* pFormat, ...);

// Функция FormatW идентична ANSI-версии функции Format, возвращает ту же
// форматированную строку, но преобразованную в Wide-строку std::wstring
std::wstring FormatW(const char* pFormat, ...);

// Пользовательская функция (callback) для FormatEx, вызывается в результате успешного форматирования
// строки. Параметр pStr указывает на начало сформированной null-terminated строки, а strLen задаёт её
// длину в символах, pData содержит значение, переданное функции FormatEx в одноимённом параметре
using FormatFnA = void(*)(const char* pStr, size_t strLen, void* pData);
using FormatFnW = void(*)(const wchar_t* pStr, size_t strLen, void* pData);

// Функция FormatEx форматирует строку. Параметр pFormat задаёт формат, а args - аргументы. В случае успешного
// форматирования строки будет вызвана пользовательская функция fmtFn, которой будет передано значение параметра
// pData, а также указатель на сформированную строку и её длина. Функция вернёт true, если форматирование было
// успешно выполнено (и функция fmtFn была вызвана). Функция вернёт false, если форматирование не было выполнено
// (в этом случае функция fmtFn не вызывается). ПРИМЕЧАНИЕ: Wide-версия функции имеет ограничение длины
// результирующей строки: если она окажется длиннее 8160 символов, то функция вернёт false
bool FormatEx(const char* pFormat, va_list args, void* pData, FormatFnA fmtFn);
bool FormatEx(const wchar_t* pFormat, va_list args, void* pData, FormatFnW fmtFn);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция Split
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
	SPLIT_TRIM					= 0x01,						// Применить к подстрокам функцию Trim
	SPLIT_ALLOW_EMPTY			= 0x02,						// Включать в результат пустые подстроки
	SPLIT_TRAILING_DELIMITER	= 0x04 + SPLIT_ALLOW_EMPTY	// Трактовать разделитель на конце str как пустую подстроку
};

// Функция Split разбивает исходную строку str на
// подстроки, используя delimiters как набор разделителей
std::vector<std::string> Split(const std::string& str, const char* pDelimiters, int flags = SPLIT_TRIM);
std::vector<std::wstring> Split(const std::wstring& str, const wchar_t* pDelimiters, int flags = SPLIT_TRIM);
std::vector<std::string> Split(const std::string& str, const std::string& delimiters, int flags = SPLIT_TRIM);
std::vector<std::wstring> Split(const std::wstring& str, const std::wstring& delimiters, int flags = SPLIT_TRIM);

} // namespace util
