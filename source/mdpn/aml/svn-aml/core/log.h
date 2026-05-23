#pragma once

#include "../../core/platform.h"
#include "../../core/util.h"
#include "singleton.h"
#include "threadsync.h"

#include <string>

namespace util {

enum LogMsgType {
	LOG_INFO = 0,	// Обычная запись журнала
	LOG_WARNING,	// Предупреждение, более важная запись
	LOG_ERROR,		// Ошибка (в данных, при обращении к ОС и т.п.), если ошибка корректно обработана
	LOG_FATAL		// Фатальная ошибка, если корректное продолжение работы невозможно
};

enum {
	LOG_SHOW_DATE	= 0x01,					 // Выводить в файл журнала текущую дату (формат YY-MM-DD)
	LOG_SHOW_TIME	= 0x02,					 // Выводить в файл журнала текущее время (формат HH:MM:SS)
	LOG_SHOW_MSEC	= 0x04 + LOG_SHOW_TIME,	 // Выводить в файл журнала текущее время с миллисекундами
	LOG_USE_UTC		= 0x08,					 // Выводить UTC время (по умолчанию выводится локальное время)
	LOG_TO_DBGR		= 0x10					 // Копировать сообщения в окно отладчика (только для SystemLog)
};

//----------------------------------------------------------------------------------------------------------------------
class LogRecord
{
	friend class Log;
	AML_NONCOPYABLE(LogRecord)

public:
	// Информирует класс журнала о завершении формирования сообщения. Если параметр flush равен true,
	// то функция не вернет управление до завершения сброса всех накопленных сообщений журнала. После
	// вызова этой функции, использование объекта (т.е. вызовы операторов << и т.п.) недопустимы.
	virtual void	End(bool flush);

	LogRecord&		operator <<(const char* pStr);
	LogRecord&		operator <<(const wchar_t* pStr);
	LogRecord&		operator <<(const std::string& str);
	LogRecord&		operator <<(const std::wstring& str);

protected:
	explicit LogRecord(Log& log);
	virtual ~LogRecord();

	virtual void	Clear();
	virtual void	Start(LogMsgType msgType, uint64_t time);

protected:
	Log&		m_Log;
	LogRecord*	m_pNext;

	bool		m_IsBusy;	// true, если объект используется (был возвращен функцией Log.StartRecord)
	bool		m_IsReady;	// true, если формирование сообщения закончено (вызвана функция End)
};

//----------------------------------------------------------------------------------------------------------------------
class AutoLogRecord
{
	AML_NONCOPYABLE(AutoLogRecord)

public:
	explicit AutoLogRecord(/* Using SystemLog */ LogMsgType msgType);
	AutoLogRecord(Log& log, LogMsgType msgType, bool flush = false);

	~AutoLogRecord()
	{
		m_Rec.End(m_Flush);
	}

	LogRecord& operator *()
	{
		return m_Rec;
	}

private:
	LogRecord&	m_Rec;
	bool		m_Flush;
};

//----------------------------------------------------------------------------------------------------------------------
class Log
{
	AML_NONCOPYABLE(Log)

public:
	Log();
	virtual ~Log();

	virtual void		SetMode(unsigned flags);
	unsigned			GetMode() const { return m_Flags; }

	// Эти функции добавляют в журнал текст msg (pMsg) в виде сообщения указанного
	// типа. Если параметр flush равен true, то функции возвращают управление только
	// после полного завершения сброса всех накопленных сообщений журнала.
	void				PutRecord(LogMsgType msgType, const char* pMsg, bool flush = false);
	void				PutRecord(LogMsgType msgType, const wchar_t* pMsg, bool flush = false);
	void				PutRecord(LogMsgType msgType, const std::string& msg, bool flush = false);
	void				PutRecord(LogMsgType msgType, const std::wstring& msg, bool flush = false);

	// Эти функции аналогичны обычным функциям PutRecord, но гарантированно не генерируют
	// исключений C++ в случаях ошибки. Также гарантируется, что при корректных значениях
	// параметра pMsg/msg структурные (SEH) исключения также не будут сгенерированы.
	void				PutRecordSafe(LogMsgType msgType, const char* pMsg, bool flush = false) throw();
	void				PutRecordSafe(LogMsgType msgType, const wchar_t* pMsg, bool flush = false) throw();
	void				PutRecordSafe(LogMsgType msgType, const std::string& msg, bool flush = false) throw();
	void				PutRecordSafe(LogMsgType msgType, const std::wstring& msg, bool flush = false) throw();

	// Возвращает ссылку на объект LogRecord для формирования записи указанного типа. После того,
	// как новая запись будет сформирована, нужно вызвать функцию LogRecord.End для добавления
	// сформированной записи в журнал и освобождения объекта LogRecord.
	virtual LogRecord&	StartRecord(LogMsgType msgType);

	// Эта функция должна вызываться из функции LogRecord::End, чтобы инициировать копирование
	// готового сообщения из объекта record во внутренний буфер (файл) журнала, после которого
	// объект record будет освобожден и помещен в стек (для повторного использования).
	virtual void		OnEndRecord(LogRecord& record);

	// Сбрасывает все накопленные записи журнала в файл (для файловых журналов, если они используют
	// отложенную запись). Функция вернет управление только после полного завершения операции.
	virtual void		Flush() {}

protected:
	virtual LogRecord*	CreateRecord();

	// Извлекает объект LogRecord из стека и возвращает указатель на этот объект, если стек
	// не пуст. Если стек пуст, то создает новый объект и помещает его в голову списка.
	LogRecord*			GetRecord();
	// Помещает указанный объект в стек.
	void				PushRecord(LogRecord* pRecord);

protected:
	unsigned				m_Flags;

	// Стек объектов LogRecord.
	unsigned				m_RecordC;
	LogRecord*				m_pHeadRecord;
	LogRecord**				m_RecordStackA;
	int						m_RecordStackTop;
	unsigned				m_RecordStackSize;
	thread::CriticalSection	m_RecordStackLock;
};

//----------------------------------------------------------------------------------------------------------------------
class FileLog : public Log
{
public:
	FileLog();
	virtual ~FileLog();

	// Инициализирует файловый журнал: если указанный файл не существует, то он создается, затем этот файл
	// открывается для записи. Если параметр append равен false, то файл очищается, и в него выводится сообщение
	// о начале новой сессии. Если append равен true, то сообщение дописывается к концу файла. Если к этому моменту
	// в журнале имеются накопленные записи, то они будут следом выведены в файл. Если в момент вызова этой функции
	// журнал уже был проинициализирован, то текущая сессия завершается и инициируется новая сессия. Если файл
	// будет успешно создан/открыт, то функция вернет true. В случае ошибки она вернет false, сессия не будет
	// начата, а сообщения продолжат накапливаться во внутреннем буфере.
	bool		Init(const std::wstring& filePath, bool append = true);
};

//----------------------------------------------------------------------------------------------------------------------
class SystemLog : public SingletonOld, public FileLog
{
	friend class SingletonOld;
	AML_SINGLETON_EX(SystemLog, {},)

public:
	// Эта функция аналогична функции Init класса FileLog за исключением того, что параметр
	// append этой функции (отсутствующий в функции класса SystemLog) всегда равен true.
	bool		Init(const std::wstring& filePath);

	// Сбрасывает все накопленные записи системного журнала в файл. Если объект системного журнала
	// (синглтон) еще не был создан или уже уничтожен, а также если системный журнал не был успешно
	// проинициализирован вызовом функции Init, функция FlushSafe ничего не сделает. В иных случаях
	// функция вернет управление только после полного завершения операции сброса данных.
	static void	FlushSafe();

private:
	static volatile bool m_IsReady;
};

//----------------------------------------------------------------------------------------------------------------------
#define AML_SYSLOG(MSG_TYPE, MSG) \
	((void)(*util::AutoLogRecord(MSG_TYPE) << MSG, 0))

} // namespace util
