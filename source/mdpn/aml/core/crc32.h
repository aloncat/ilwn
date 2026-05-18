//⬪AML⬪
#pragma once

#include "platform.h"

namespace hash {

// Вычисляет контрольную сумму CRC32 блока данных pData размером size
// байт. Параметр prevHash используется для инкрементного вычисления хеша
uint32_t GetCRC32(const void* pData, size_t size, uint32_t prevHash = 0);

} // namespace hash
