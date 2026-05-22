//∙MDPN
#pragma once

#include <core/forward.h>
#include <core/platform.h>

#include <functional>
#include <string>

class Number;

// Вызывает функцию fn внутри блока try..except, отлавливая все типы исключений. Если произойдёт
// исключение, то в консоль будет выведено сообщение и функция вернёт значение errorCode. Иначе
// будет возращено значение, возвращённое функцией fn. Если fn не задана, то функция вернёт 0
int GuardedCall(const std::function<int()>& fn, int errorCode = 1);

// Сжимает данные из файла src (начиная с текущей позиции до конца файла) и сохраняет их в файл dst,
// начиная с текущей позиции. Параметр level задаёт степень сжатия от 0 до 9, где 0 соответствует
// отсутствию сжатия, 1 - самому слабому (быстрому), а 9 - самому сильному (медленному) сжатию
bool CompressFile(util::File& src, util::File& dst, int level = 5);
// Разжимает сжатые (функцией CompressFile) данные из файла src (начиная с текущей
// позиции до конца файла) и сохраняет их в файл dst, начиная с текущей позиции
bool DecompressFile(util::File& src, util::File& dst);

// Возвращает true, если pStr содержит только цифры. Если
// pStr указывает на пустую строку, функция вернёт false
bool IsNumber(const char* pStr);
// Возвращает true, если первые count символов строки pStr являются цифрами.
// Если строка пуста или содержит менее count символов, функция вернёт false
bool IsNumber(const char* pStr, size_t count);

bool ConvertToNumber(std::wstring_view s, Number& number);

// Возвращает процентное (0..100) отношение значения count к полному
// количеству чисел, подлежащих проверке в указанном диапазоне range
float GetRangeProgress(size_t range, uint64_t count);

// Форматирует число, разделяя группы цифр указанным символом
std::string SeparateWithCommas(uint64_t number, char separator = ',');
std::string SeparateWithCommas(const Number& number, char separator = ',');
// Возвращает строку, символы которой разделены запятыми на группы по три
std::string SeparateWithCommas(const std::string& s, char separator = ',');

// Возвращает строку вида "X.YYZ/s" или "XX.YZ/s", форматируя значение speed как кол-во
// операций в секунду (где Z может принимать одно из значений: "K", "M", "G" или "T")
std::string FormatSpeed(float speed);

// Возвращает строку вида "X.YY ZiB", "XX.Y ZiB" или "XXX ZiB", форматируя значение как размер
// в *байтах (где Z может принимать одно из значений: "K", "M", "G" или "T"). Если параметр
// colored == true, то вместо пробела/суффикса будет суффикс с маркерами цвета "#8ZiB#7"
std::string FormatSize(uint64_t sizeInBytes, bool colored = false);

// Возвращает строку, содержащую указанное количество символов '\b' и пробелов.
// Если twice == true, то в конце добавляет ещё столько же символов '\b'
std::string EraseTextSequence(size_t charsToErase, bool twice = false);
