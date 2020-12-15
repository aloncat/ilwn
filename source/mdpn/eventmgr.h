//⬪MDPN⬪
#pragma once

#include <core/threadsync.h>
#include <core/util.h>

#include <atomic>
#include <string>
#include <vector>

class DataBase;
class Number;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   EventManager - менеджер событий
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class EventManager final
{
	AML_NONCOPYABLE(EventManager)

public:
	EventManager(const DataBase& data);

	// Регистрирует событие нахождения палиндрома для нового шага
	void OnPalindromeFound(const Number& num, unsigned step);
	// Регистрирует событие изменения глубины поиска
	void OnSearchDepthChanged(unsigned newDepth);
	// Регистрирует событие завершения диапазона
	void OnRangeCompleted(size_t digitC);
	// Регистрирует пользовательское событие с текстом text
	void OnCustomEvent(const std::string& text, bool logFileOnly = false);

	// Возвращает true, если есть невыведенные сообщения. Если параметр noLogOnly == false,
	// то функция вернёт true, если в очереди есть любое сообщение. Если noLogOnly == true,
	// то функция проверит только наличие сообщений, предназначенных для вывода в окно консоли
	bool HasEvents(bool noLogOnly = false) const { return m_EventC && (!noLogOnly || HasConsoleEvents()); }

	// Выводит сообщение о событии на экран (опционально) и в файл журнала
	static void PublishEvent(const std::string& msg, bool logFileOnly = false);
	// Выводит на экран и в файл журнала все накопленные события
	void PublishAll();

private:
	struct Event {
		std::string text;
		bool logFileOnly = false;
	};

	void AddEvent(Event&& event);
	bool HasConsoleEvents() const;

	const DataBase& m_Data;
	std::vector<Event> m_Events;
	mutable thread::CriticalSection m_CS;
	std::atomic<size_t> m_EventC = 0;
	unsigned m_HighestStep = 0;
};
