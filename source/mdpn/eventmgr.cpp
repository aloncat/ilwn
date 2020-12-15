//⬪MDPN⬪
#include "pch.h"
#include "eventmgr.h"

#include "dbase.h"
#include "log.h"
#include "stephlp.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/datetime.h>
#include <core/platform.h>
#include <core/strutil.h>

#include <algorithm>

//----------------------------------------------------------------------------------------------------------------------
EventManager::EventManager(const DataBase& data)
	: m_Data(data)
	, m_CS(500)
{
	m_Events.reserve(15);
	m_HighestStep = m_Data.GetHighestStep();
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::OnPalindromeFound(const Number& num, unsigned step)
{
	m_HighestStep = std::max(StepHelper::GetHighest(num), m_HighestStep);
	const bool isRecord = step > m_HighestStep;

	if (isRecord)
		m_HighestStep = step;

	Event event;
	event.text = util::Format(isRecord ? "New #12RECORD!#7 found: number #10#%s#7"
		" for step #10#%u" : "Found the smallest number #15#%s#7 for step #15#%u",
		SeparateWithCommas(num).c_str(), step);

	AddEvent(std::move(event));
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::OnSearchDepthChanged(unsigned newDepth)
{
	Event event;
	event.text = util::Format("#6Current search depth was changed to #12#%u#6 steps", newDepth);

	AddEvent(std::move(event));
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::OnRangeCompleted(size_t digitC)
{
	if (m_Data.HasGaps())
	{
		Event event;
		event.text = util::Format("#12WARNING: #10#%u#3-digit range has"
			" one or more unsearched gaps!", digitC);
		AddEvent(std::move(event));
	}

	const uint64_t lychrelC = m_Data.GetLychrelC(digitC);
	const float lychrelRatio = GetRangeProgress(digitC, lychrelC);

	Event event;
	event.text = util::Format(m_Data.HasGaps() ? "#3Searching done for #10#%u#3-digit range" :
		"#3Searching done for all #10#%u#3-digit numbers", digitC);
	event.text += util::Format(lychrelC ? m_Data.HasGaps() ? " (at least #12~%.2f%%#3 Lychrels)" :
		" (#12~%.2f%%#3 Lychrels)" : " (no Lychrels)", lychrelRatio);

	AddEvent(std::move(event));
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::OnCustomEvent(const std::string& text, bool logFileOnly)
{
	Event event;
	event.text = text;
	event.logFileOnly = logFileOnly;

	AddEvent(std::move(event));
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::PublishEvent(const std::string& msg, bool logFileOnly)
{
	util::DateTime dateTime;
	dateTime.Update(false);

	std::string s = util::Format("\r#2[%02u-%02u-%02u %02u:%02u:%02u]#7 ",
		dateTime.year % 100, dateTime.month, dateTime.day,
		dateTime.hour, dateTime.minute, dateTime.second);

	s += msg;
	s += '\n';

	if (!logFileOnly)
		aux::Printc(s);
	SystemLog::Instance().Print(s);
}

//----------------------------------------------------------------------------------------------------------------------
void EventManager::PublishAll()
{
	if (m_EventC)
	{
		std::vector<Event> events;
		events.reserve(15);

		{
			thread::Lock<thread::CriticalSection> lock(m_CS);
			// Внутри критической секции нет необходимости в атомарности,
			// используем memory_order_relaxed, чтобы не блокировать шину
			m_EventC.store(0, std::memory_order_relaxed);
			m_Events.swap(events);
		}

		for (const auto& e : events)
			PublishEvent(e.text, e.logFileOnly);
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void EventManager::AddEvent(Event&& event)
{
	thread::Lock<thread::CriticalSection> lock(m_CS);
	m_Events.push_back(std::move(event));

	// Мы всё ещё внутри критической секции, никто не сможет изменить m_EventC в данный
	// момент. Поэтому нет необходимости в атомарности (блокировке шины префиксом lock)
	m_EventC.store(m_EventC + 1, std::memory_order_relaxed);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool EventManager::HasConsoleEvents() const
{
	if (m_EventC)
	{
		thread::Lock<thread::CriticalSection> lock(m_CS);
		for (const auto& e : m_Events)
		{
			if (!e.logFileOnly)
				return true;
		}
	}
	return false;
}
