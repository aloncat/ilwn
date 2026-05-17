//∙MDPN
#pragma once

#include <string>

// Возвращает строку, содержащую версию программы: дату и время в формате "YYYY-MM-DD HH:MM". Дата и время
// берётся из макросов __DATE__ и __TIME__ соответственно. В отладочных сборках возвращает "[DEBUG]"
std::string GetAppVersion();
