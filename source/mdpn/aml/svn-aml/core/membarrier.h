#pragma once

#include "../../core/platform.h"

#if AML_OS_WINDOWS
	#include <intrin.h>
#endif

namespace thread {

//----------------------------------------------------------------------------------------------------------------------
inline void ReadMemBarrier()
{
	#if AML_OS_WINDOWS
		// На платформе Windows нет специальных барьеров чтения/записи для
		// процессора. Достаточно соответствующего барьера компилятора.
		_ReadBarrier();
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
inline void WriteMemBarrier()
{
	#if AML_OS_WINDOWS
		// На платформе Windows нет специальных барьеров чтения/записи для
		// процессора. Достаточно соответствующего барьера компилятора.
		_WriteBarrier();
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
inline void FullMemBarrier()
{
	#if AML_OS_WINDOWS
		// На платформе Windows интринсики _Interlocked являются
		// полными барьерами памяти компилятора и процессора.
		volatile long mem;
		_InterlockedExchange(&mem, 0);
	#else
		#error Not implemented
	#endif
}

} // namespace thread
