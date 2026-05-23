//⬪AML⬪
#include "pch.h"
#include "thread.h"

#include "platform.h"
#include "winapi.h"

#if AML_OS_WINDOWS
	#include "intrin.h"
#endif

namespace thread {

//----------------------------------------------------------------------------------------------------------------------
void CPUPause()
{
	#if AML_OS_WINDOWS
		// Инструкция pause относится к набору SSE2, который впервые появился в Pentium 4. Поддержка SSE2
		// требуется для установки Windows 8 и более новых ОС компании Microsoft. Также SSE2 присутствует
		// на всех 64-битных процессорах Intel и AMD. Но несмотря на то, что инструкция pause появилась в
		// наборе SSE2, на процессорах, не поддерживающих SSE2, она эквивалентна обычной инструкции nop.
		// Т.о. использование этой инструкции не требует обязательной поддержки SSE2 процессором
		_mm_pause();
	#else
		#error Not implemented
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
void Sleep(unsigned milliseconds)
{
	#if AML_OS_WINDOWS
		::Sleep(milliseconds);
	#else
		#error Not implemented
	#endif
}

} // namespace thread
