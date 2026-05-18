//⬪AML⬪
#include "pch.h"
#include "util.h"

namespace util {

//----------------------------------------------------------------------------------------------------------------------
static inline bool CheckCompiler()
{
	// В некоторых местах в коде проекта мы подразумеваем, что размер типа char
	// равен 1 байту. Если это не так, то наш код не сможет работать корректно
	static_assert(sizeof(char) == 1, "Unsupported size of char type");

	// На Windows размер символов Wide-строк равен 16 битам, на Unix и ОС этого
	// семейства он равен 32 битам. Наш код знает только об этих 2 вариантах
	static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
		"Unsupported size of wchar_t type");

	// В коде проекта при использовании типа int (unsigned) мы никогда не опираемся
	// на предположение о каком-либо конкретном размере этого типа. Однако, если
	// его размер будет меньше 32 бит, наш код не сможет работать корректно
	static_assert(sizeof(int) >= 4, "Size of int type must be at least 32 bits");

	// Для всех платформ, на которых этот проект может быть использован, размер
	// типа long long равен 64 битам. Но на всякий случай проверим этот размер
	static_assert(sizeof(long long) == 8, "Unsupported size of long long type");

	// В теории на какой-нибудь платформе размерность указателей может отличаться
	// от размера типа size_t. Если так, то некоторые трюки в коде будут некорректны
	static_assert(sizeof(size_t) == sizeof(void*), "Size differs for size_t and void* types");

	#if AML_64BIT
		// Проверим соответствие макроса AML_64BIT и размера регистра. Если
		// этот макрос не эквивалентен true, то платформа (target) 32-битная
		static_assert(sizeof(size_t) == 8, "Incorrect size of size_t type");
	#else
		static_assert(sizeof(size_t) == 4, "Incorrect size of size_t type");
	#endif

	#if AML_OS_WINDOWS
		// На платформе Windows (а именно для вызовов WinAPI) тип long должен быть 32-битным.
		// Также мы иногда используем тип unsigned для хранения 32-битных значений типа DWORD.
		// Предположительно, данные условия будут выполняться на всех компиляторах под Windows
		static_assert(sizeof(unsigned) == 4, "Incorrect size of unsigned (int) type");
		static_assert(sizeof(long) == 4, "Incorrect size of long type");
	#endif

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
static bool CheckEndianness()
{
	const uint32_t value = 1;
	const uint8_t* pFirstByte = reinterpret_cast<const uint8_t*>(&value);

	#if AML_LITTLE_ENDIAN
		return *pFirstByte != 0;
	#elif AML_BIG_ENDIAN
		return *pFirstByte == 0;
	#else
		return false;
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool CheckMinimalRequirements(bool terminateIfFailed)
{
	static bool isDone = false;
	if (isDone || (CheckCompiler() && CheckEndianness()))
	{
		isDone = true;
		return true;
	}

	if (terminateIfFailed)
		abort();

	return false;
}

} // namespace util
