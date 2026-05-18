//⬪AML⬪
#include "pch.h"
#include "console.h"

#include "array.h"
#include "exception.h"
#include "winapi.h"

using namespace util;

// TODO: класс Console имеет глобальные недоработки: конструктор всегда подключается к стандартной
// консоли (т.е. ведёт себя корректно только для случая единственного объекта Console в консольном
// приложении). Для GUI приложения окно консоли сейчас вообще не создаётся

AML_SINGLETON(SystemConsole)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console::CtrlHandler - Windows
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//----------------------------------------------------------------------------------------------------------------------
struct Console::CtrlHandler
{
	using HandlerFn = BOOL(WINAPI*)(DWORD);

	// Резервирует обработчик событий консольного окна и возвращает указатель на него. Параметр pFlag должен указывать
	// на переменную типа bool, которой обработчиком будет присвоено значение true, если пользователь нажмёт Ctrl-C
	static HandlerFn GetHandler(volatile bool* pFlag)
	{
		static bool isInitialized = Init();
		if (!pFlag || !s_pCS)
			return nullptr;

		thread::CriticalSection::Lock lock(s_pCS);
		for (size_t idx = 0; idx < MAX_HANDLER_C; ++idx)
		{
			if (!s_FlagA[idx])
			{
				s_FlagA[idx] = pFlag;
				switch (idx)
				{
					case 0: return HandlerN<0>;
					case 1: return HandlerN<1>;
					case 2: return HandlerN<2>;
					case 3: return HandlerN<3>;
					case 4: return HandlerN<4>;
					case 5: return HandlerN<5>;

					default:
						// TODO: это нужно делать через Assert. Но
						// пока в AML отсутствует обработка ошибок
						throw util::EAssertion("Incomplete switch statement");
				}
			}
		}
		// Если свободных обработчиков нет, то вернём "общий"
		// обработчик, который не устанавливает никакой флаг
		return HandlerN<MAX_HANDLER_C>;
	}

	// Освобождает зарезервированный ранее обработчик для указанной переменной pFlag
	static void ReleaseHandler(volatile bool* pFlag)
	{
		if (pFlag && s_pCS)
		{
			thread::CriticalSection::Lock lock(s_pCS);
			for (size_t idx = 0; idx < MAX_HANDLER_C; ++idx)
			{
				if (s_FlagA[idx] == pFlag)
					s_FlagA[idx] = nullptr;
			}
		}
	}

protected:
	static constexpr size_t MAX_HANDLER_C = 6;

	template<size_t idx> static BOOL WINAPI HandlerN(DWORD ctrlType)
	{
		return Handler(idx, ctrlType);
	}

	static AML_NOINLINE BOOL Handler(size_t index, DWORD ctrlType)
	{
		if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
		{
			if (index < MAX_HANDLER_C && s_pCS)
			{
				thread::CriticalSection::Lock lock(s_pCS);
				if (volatile bool* pFlag = s_FlagA[index])
					*pFlag = true;
			}
			return TRUE;
		}
		return FALSE;
	}

	static bool Init()
	{
		static thread::CriticalSection cs;
		atexit([]() { s_pCS = nullptr; });
		s_pCS = &cs;
		return true;
	}

	static volatile bool* s_FlagA[MAX_HANDLER_C];
	static thread::CriticalSection* s_pCS;
};

volatile bool* Console::CtrlHandler::s_FlagA[MAX_HANDLER_C];
thread::CriticalSection* Console::CtrlHandler::s_pCS = nullptr;

#endif // AML_OS_WINDOWS

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - общие для всех платформ функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const char* pStr, int color)
{
	Write(pStr, pStr ? strlen(pStr) : 0, color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const wchar_t* pStr, int color)
{
	Write(pStr, pStr ? wcslen(pStr) : 0, color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const std::string& str, int color)
{
	Write(str.c_str(), str.size(), color);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const std::wstring& str, int color)
{
	Write(str.c_str(), str.size(), color);
}

//----------------------------------------------------------------------------------------------------------------------
bool Console::IsCtrlCPressed(bool reset)
{
	PollInput();
	if (m_IsCtrlCPressed)
	{
		m_IsCtrlCPressed = !reset;
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - Windows
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//----------------------------------------------------------------------------------------------------------------------
Console::Console()
{
	HANDLE outHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (outHandle && outHandle != INVALID_HANDLE_VALUE)
	{
		DWORD mode;
		m_Info.pOutHandle = outHandle;
		m_Info.isRedirected = ::GetConsoleMode(outHandle, &mode) == 0;

		if (!m_Info.isRedirected)
		{
			CONSOLE_SCREEN_BUFFER_INFO info;
			if (::GetConsoleScreenBufferInfo(m_Info.pOutHandle, &info))
				m_Info.oldTextColor = info.wAttributes & 0xff;

			HANDLE inHandle = ::GetStdHandle(STD_INPUT_HANDLE);
			if (inHandle && inHandle != INVALID_HANDLE_VALUE)
			{
				m_Info.pInHandle = inHandle;
				if (::GetConsoleMode(inHandle, &mode))
				{
					mode &= ~ENABLE_PROCESSED_INPUT;
					::SetConsoleMode(inHandle, mode);
				}
			}

			if (auto pHandler = CtrlHandler::GetHandler(&m_IsCtrlCPressed))
			{
				m_Info.pCtrlHandler = pHandler;
				::SetConsoleCtrlHandler(pHandler, TRUE);
			}
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
Console::~Console()
{
	if (m_Info.pOutHandle && !m_Info.isRedirected)
	{
		if (auto pHandler = static_cast<CtrlHandler::HandlerFn>(m_Info.pCtrlHandler))
		{
			::SetConsoleCtrlHandler(pHandler, FALSE);
			CtrlHandler::ReleaseHandler(&m_IsCtrlCPressed);
		}

		WORD attr = static_cast<WORD>(m_Info.oldTextColor);
		::SetConsoleTextAttribute(m_Info.pOutHandle, attr);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool Console::GetInputEvent(KeyEvent& event)
{
	thread::CriticalSection::Lock lock(m_InputCS);

	if (!m_InputEvents.empty())
	{
		event = m_InputEvents.front();
		m_InputEvents.pop_front();
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(const std::string& title)
{
	if (m_Info.pOutHandle && !m_Info.isRedirected)
		::SetConsoleTitleA(title.c_str());
}

//----------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(const std::wstring& title)
{
	if (m_Info.pOutHandle && !m_Info.isRedirected)
		::SetConsoleTitleW(title.c_str());
}

//----------------------------------------------------------------------------------------------------------------------
void Console::SetColor(int color)
{
	color &= 0xff;
	if (m_TextColor != color)
	{
		m_TextColor = color;
		WORD attr = static_cast<WORD>(color);
		::SetConsoleTextAttribute(m_Info.pOutHandle, attr);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Console::Write(const char* pStr, size_t strLen, int color)
{
	if (!m_Info.pOutHandle || !pStr || !strLen || strLen > INT_MAX)
		return;

	thread::CriticalSection::Lock lock(m_OutputCS);

	if (m_Info.isRedirected)
	{
		DWORD byteC = static_cast<DWORD>(strLen);
		::WriteFile(m_Info.pOutHandle, pStr, byteC, &byteC, nullptr);
	} else
	{
		util::SmartArray<char> buffer(strLen + 1);
		::CharToOemA(pStr, buffer);

		SetColor(color);
		DWORD charC = static_cast<DWORD>(strLen);
		::WriteConsoleA(m_Info.pOutHandle, buffer, charC, &charC, nullptr);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Console::Write(const wchar_t* pStr, size_t strLen, int color)
{
	if (!m_Info.pOutHandle || !pStr || !strLen || strLen > INT_MAX)
		return;

	thread::CriticalSection::Lock lock(m_OutputCS);

	if (m_Info.isRedirected)
	{
		int len = static_cast<int>(strLen);
		int bufferLen = ::WideCharToMultiByte(CP_ACP, 0, pStr, len, nullptr, 0, nullptr, nullptr);
		if (bufferLen > 0)
		{
			util::SmartArray<char> buffer(bufferLen);
			len = ::WideCharToMultiByte(CP_ACP, 0, pStr, len, buffer, bufferLen, nullptr, nullptr);
			if (len > 0)
			{
				DWORD bytesWritten;
				::WriteFile(m_Info.pOutHandle, buffer, len, &bytesWritten, nullptr);
			}
		}
	} else
	{
		SetColor(color);
		DWORD charC = static_cast<DWORD>(strLen);
		::WriteConsoleW(m_Info.pOutHandle, pStr, charC, &charC, nullptr);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool Console::CheckPollTime()
{
	const DWORD timeStamp = ::GetTickCount();
	// NB: значение, возвращаемое функцией GetTickCount, обычно меняется с интервалом около
	// 15ms. Поэтому события ввода будут обрабатываться не чаще, чем раз в примерно 15 ms
	bool canPoll = m_LastPollTime != timeStamp;
	m_LastPollTime = timeStamp;
	return canPoll;
}

//----------------------------------------------------------------------------------------------------------------------
void Console::PollInput()
{
	if (!m_Info.pInHandle)
		return;

	thread::CriticalSection::Lock lock(m_InputCS);

	DWORD totalEventC = 0;
	if (CheckPollTime() && ::GetNumberOfConsoleInputEvents(m_Info.pInHandle, &totalEventC) && totalEventC)
	{
		const DWORD MAX_EVENT_C = 24;
		INPUT_RECORD eventBuffer[MAX_EVENT_C];

		while (totalEventC)
		{
			DWORD eventC = 0;
			if (::ReadConsoleInputA(m_Info.pInHandle, eventBuffer, MAX_EVENT_C, &eventC) == 0 || !eventC)
				break;

			totalEventC -= (eventC <= totalEventC) ? eventC : totalEventC;

			for (unsigned i = 0; i < eventC; ++i)
			{
				INPUT_RECORD& event = eventBuffer[i];
				if (event.EventType == KEY_EVENT)
				{
					const WORD key = event.Event.KeyEvent.wVirtualKeyCode;
					const bool isKeyDown = event.Event.KeyEvent.bKeyDown == TRUE;
					const DWORD ctrlState = event.Event.KeyEvent.dwControlKeyState;
					const bool isCtrlDown = (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

					if (key == 'C' && isKeyDown && isCtrlDown)
						m_IsCtrlCPressed = true;
					else
					{
						// Если буфер заполнен, удалим самое старое событие
						if (m_InputEvents.size() >= MAX_KEYEVENT_C)
							m_InputEvents.pop_front();

						// TODO: такая обработка событий ввода не совсем корректна, так как мы не учитываем значение
						// повторов event.Event.KeyEvent.wRepeatCount. И ещё, было бы неплохо сделать свой enum с
						// кодами, чтобы унифицировать их для разных платформ как в подсистеме ввода Sparky
						m_InputEvents.push_back(KeyEvent { key, isKeyDown, isCtrlDown });
					}
				}
			}
		}
	}
}

#endif // AML_OS_WINDOWS

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - заглушка для всех остальных платформ
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !AML_OS_WINDOWS
Console::Console() {}
Console::~Console() {}
bool Console::GetInputEvent(KeyEvent& event) { return false; }
void Console::SetTitle(const std::string& title) {}
void Console::SetTitle(const std::wstring& title) {}
void Console::SetColor(int color) {}
void Console::Write(const char* pStr, size_t strLen, int color) {}
void Console::Write(const wchar_t* pStr, size_t strLen, int color) {}
bool Console::CheckPollTime() { return false; }
void Console::PollInput() {}
#endif
