//⬪AML⬪
#pragma once

#include <limits.h>
#include <stdint.h>

// NB: все макросы AML (кроме макросов Assert, Verify и Halt) должны начинаться с префикса AML_ и быть записаны
// в верхнем регистре. Также любой макрос всегда должен быть определён (в основном либо как 1, либо как 0)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Проверка платформы и конфигурации
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && defined(_WIN32)
	#define AML_OS_WINDOWS 1
	#if _MSC_VER < 1900
		// Проект AML требует поддержки стандарта C11/C++14
		#error Microsoft Visual C++ 2015 or newer is required
	#endif
#else
	#define AML_OS_WINDOWS 0
	#error Unrecognized compiler or platform
#endif

#if defined(_MSC_VER) && defined(_WIN64)
	#define AML_64BIT 1
#else
	#define AML_64BIT 0
#endif

#ifdef AML_USEASM
	#undef AML_USEASM
	#define AML_USEASM 1
	#if defined(_MSC_VER) && defined(_M_IX86_FP) && _M_IX86_FP < 2
		// Ассемблерный код проекта использует инструкции SSE2 и поэтому, если макрос AML_USEASM определён
		// (и вместо C++ кода используются asm-версии функций), то нет смысла отключать расширенный набор
		// инструкций ключом компилятора /arch (по умолчанию VS2015 использует набор инструкций SSE2).
		// Если инструкции SSE2 отключены, просто выведем сообщение (предупреждение) в лог компилятора
		#pragma message("WARNING: SSE2 is required for AML project, change or remove /arch compiler option")
	#endif
#else
	#define AML_USEASM 0
#endif

#if (defined(DEBUG) || defined(_DEBUG)) && defined(NDEBUG)
	// Макросы DEBUG/_DEBUG не являются стандартными. MSVC автоматически определяет _DEBUG, если
	// используется отладочная версия RTL. Макрос NDEBUG, напротив, стандартен, и используется для
	// выключения отладочной функциональности. Для релизных конфигураций в настройках проекта должен
	// быть объявлен макрос NDEBUG. Для отладочных конфигураций объявлять макрос _DEBUG не нужно
	#error Both DEBUG/_DEBUG and NDEBUG macros are defined
#elif defined(NDEBUG)
	#define AML_DEBUG 0
#else
	#define AML_DEBUG 1
#endif

#ifdef AML_PRODUCTION
	#ifdef NDEBUG
		#undef AML_PRODUCTION
		#define AML_PRODUCTION 1
	#else
		// Макрос AML_PRODUCTION должен быть опеределён вместе с маросом NDEBUG. Как правило, этот макрос
		// используется для production сборки. Он полностью отключает обработку ошибок в Assert/Verify
		#error Production target must have NDEBUG macro defined
	#endif
#else
	#define AML_PRODUCTION 0
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Настройка компилятора
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	#pragma warning(disable: 4100)	// C4100 (L4): "unreferenced formal parameter"
	#pragma warning(disable: 4127)	// C4127 (L4): "conditional expression is constant"
	#pragma warning(3:		 4456)	// C4456 (L4): "declaration of 'identifier' hides previous local declaration"
	#pragma warning(3:		 4457)	// C4457 (L4): "declaration of 'identifier' hides function parameter"
	#pragma warning(3:		 4458)	// C4458 (L4): "declaration of 'identifier' hides class member"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы для платформы Windows
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	#define AML_CDECL __cdecl
	#define AML_FASTCALL __fastcall
	#define AML_STDCALL __stdcall

	#define AML_NOINLINE __declspec(noinline)

	#define AML_LITTLE_ENDIAN 1
	#define AML_BIG_ENDIAN 0
#endif
