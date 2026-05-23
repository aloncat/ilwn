#include "log.h"

#include "../../core/exception.h"
#include "datetime.h"
#include "debug.h"
#include "error.h"
#include "membarrier.h"

using namespace util;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макрос LOG_ASSERT
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NDEBUG
	#define LOG_ASSERT(EXPRESSION) ((void) 0)
#else
	#define LOG_ASSERT(EXPRESSION) ((void)(!(EXPRESSION) && (ErrorHelper::AssertNoLog( \
		AML_WSTRING(__FILE__), __LINE__, AML_WSTRING(#EXPRESSION)), AML_DBGBREAK(), 0)))
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogRecord
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
LogRecord::LogRecord(Log& log)
	: m_Log(log)
	, m_pNext(0)
	, m_IsBusy(false)
	, m_IsReady(false)
{
}

//----------------------------------------------------------------------------------------------------------------------
LogRecord::~LogRecord()
{
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const char* pStr)
{
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const wchar_t* pStr)
{
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const std::string& str)
{
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const std::wstring& str)
{
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
void LogRecord::Clear()
{
	m_IsBusy = false;
	m_IsReady = false;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void LogRecord::End(bool flush)
{
	m_IsReady = true;
	m_Log.OnEndRecord(*this);
	if (flush) m_Log.Flush();
}

//----------------------------------------------------------------------------------------------------------------------
void LogRecord::Start(LogMsgType msgType, uint64_t time)
{
	m_IsBusy = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   AutoLogRecord
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE AutoLogRecord::AutoLogRecord(LogMsgType msgType)
	: m_Rec(SystemLog::Instance().StartRecord(msgType))
	, m_Flush(false)
{
}

//----------------------------------------------------------------------------------------------------------------------
AutoLogRecord::AutoLogRecord(Log& log, LogMsgType msgType, bool flush)
	: m_Rec(log.StartRecord(msgType))
	, m_Flush(flush)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Log
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
Log::Log()
	: m_Flags(LOG_SHOW_TIME)
	, m_RecordC(0)
	, m_pHeadRecord(0)
	, m_RecordStackA(0)
	, m_RecordStackTop(-1)
	, m_RecordStackSize(0)
	, m_RecordStackLock(100)
{
	if (DebugHelper::Instance().IsDebuggerActive())
		m_Flags |= LOG_TO_DBGR;
}

//----------------------------------------------------------------------------------------------------------------------
Log::~Log()
{
	AML_SAFE_DELETEA(m_RecordStackA);
	for (LogRecord* pRec = m_pHeadRecord; pRec;)
	{
		LogRecord* pTemp = pRec;
		pRec = pRec->m_pNext;
		delete pTemp;
	}
}

//----------------------------------------------------------------------------------------------------------------------
LogRecord* Log::CreateRecord()
{
	return new LogRecord(*this);
}

//----------------------------------------------------------------------------------------------------------------------
LogRecord* Log::GetRecord()
{
	thread::CriticalSection::Lock lock(m_RecordStackLock);

	// Если стек не пуст, извлекаем объект из него.
	if (m_RecordStackTop >= 0)
		return m_RecordStackA[m_RecordStackTop--];

	// Стек пуст, нам нужно создать новый объект. Если количество уже созданных объектов велико,
	// значит либо что-то неправильно в обработке сформированных сообщений, либо где-то утечка.
	LOG_ASSERT((m_RecordC < 1000000) && "Too many log records in use");

	if (m_RecordC < INT_MAX)
	{
		if (m_RecordC >= m_RecordStackSize)
		{
			LogRecord** pOldStack = m_RecordStackA;
			unsigned newStackSize = m_RecordStackSize + 1024;
			m_RecordStackA = new LogRecord*[newStackSize];
			m_RecordStackSize = newStackSize;
			AML_SAFE_DELETEA(pOldStack);
		}
		if (LogRecord* pRec = CreateRecord())
		{
			++m_RecordC;
			pRec->m_pNext = m_pHeadRecord;
			m_pHeadRecord = pRec;
			return pRec;
		}
	}
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
void Log::OnEndRecord(LogRecord& record)
{
	record.Clear();
	PushRecord(&record);
}

//----------------------------------------------------------------------------------------------------------------------
void Log::PushRecord(LogRecord* pRecord)
{
	m_RecordStackLock.Enter();
	// При создании нового объекта мы всегда проверяем размер стека и, если нужно, то
	// увеличиваем его до такого размера, чтобы он мог вместить все созданные объекты.
	m_RecordStackA[++m_RecordStackTop] = pRecord;
	m_RecordStackLock.Leave();
}

//----------------------------------------------------------------------------------------------------------------------
#define PUT_RECORD(MSG) {						\
	AutoLogRecord rec(*this, msgType, flush);	\
	*rec << MSG;								\
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecord(LogMsgType msgType, const char* pMsg, bool flush)
{
	if (pMsg && *pMsg) PUT_RECORD(pMsg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecord(LogMsgType msgType, const wchar_t* pMsg, bool flush)
{
	if (pMsg && *pMsg) PUT_RECORD(pMsg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecord(LogMsgType msgType, const std::string& msg, bool flush)
{
	if (!msg.empty()) PUT_RECORD(msg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecord(LogMsgType msgType, const std::wstring& msg, bool flush)
{
	if (!msg.empty()) PUT_RECORD(msg);
}

//----------------------------------------------------------------------------------------------------------------------
#define PUT_RECORD_SAFE(MSG) {					\
	try {										\
		LogRecord& rec = StartRecord(msgType);	\
		try { rec << MSG; } catch (...) {}		\
		rec.End(flush);							\
	} catch (...) {}							\
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecordSafe(LogMsgType msgType, const char* pMsg, bool flush) throw()
{
	if (pMsg && *pMsg) PUT_RECORD_SAFE(pMsg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecordSafe(LogMsgType msgType, const wchar_t* pMsg, bool flush) throw()
{
	if (pMsg && *pMsg) PUT_RECORD_SAFE(pMsg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecordSafe(LogMsgType msgType, const std::string& msg, bool flush) throw()
{
	if (!msg.empty()) PUT_RECORD_SAFE(msg);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Log::PutRecordSafe(LogMsgType msgType, const std::wstring& msg, bool flush) throw()
{
	if (!msg.empty()) PUT_RECORD_SAFE(msg);
}

//----------------------------------------------------------------------------------------------------------------------
void Log::SetMode(unsigned flags)
{
	m_Flags = flags;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& Log::StartRecord(LogMsgType msgType)
{
	uint64_t time = (m_Flags & (LOG_SHOW_DATE | LOG_SHOW_TIME)) ?
		DateTime::Now((m_Flags & LOG_USE_UTC) != 0) : 0;

	LogRecord* pRec = GetRecord();
	LOG_ASSERT((pRec != 0) && "Failed to get LogRecord");
	if (!pRec) throw EHalt("Failed to get LogRecord");

	try
	{
		pRec->Start(msgType, time);
		return *pRec;
	}
	catch (...)
	{
		pRec->Clear();
		PushRecord(pRec);
		throw;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileLog
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
FileLog::FileLog()
{
}

//----------------------------------------------------------------------------------------------------------------------
FileLog::~FileLog()
{
}

//----------------------------------------------------------------------------------------------------------------------
bool FileLog::Init(const std::wstring& filePath, bool append)
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemLog
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

AML_IMPLEMENT_SINGLETON(SystemLog);

volatile bool SystemLog::m_IsReady = false;

//----------------------------------------------------------------------------------------------------------------------
SystemLog::~SystemLog()
{
	m_IsReady = false;
}

//----------------------------------------------------------------------------------------------------------------------
void SystemLog::FlushSafe()
{
	if (m_pInstance && m_IsReady)
	{
		try
		{
			thread::ReadMemBarrier();
			Instance().Flush();
		}
		catch (...) {}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool SystemLog::Init(const std::wstring& filePath)
{
	bool ok = FileLog::Init(filePath, true);
	thread::WriteMemBarrier();
	m_IsReady = ok;
	return ok;
}
