//⬪MDPN⬪
#pragma once

#include <core/file.h>
#include <core/singleton.h>
#include <core/threadsync.h>
#include <core/util.h>

#include <functional>
#include <string>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogFile - файл журнала
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class LogFile
{
	AML_NONCOPYABLE(LogFile)

public:
	explicit LogFile(const std::wstring& filePath = L"log.txt");
	~LogFile();

	void Print(const char* pMsg);
	void Print(const std::string& msg);

	void Close();

protected:
	// Убирает из исходной строки теги раскраски текста и управляющие последовательности
	static void Sanitize(const char* pStr, size_t strLen, const std::function<void(const char*, size_t)>& cb);

	util::BinaryFile m_File;
	thread::CriticalSection m_CS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemLog - синглтон системного журнала
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SystemLog : public LogFile, public util::Singleton<SystemLog>
{
public:
	// Задаёт пользовательский путь к файлу журнала. Функция должна вызываться
	// до первого обращения к синглтону (т.е. до создания самого журнала)
	static void SetPath(const std::wstring& filePath);

protected:
	SystemLog();
	virtual ~SystemLog() override = default;

	virtual void OnDestroy() override { Tidy(); }

private:
	static void Tidy();
	static std::wstring GetFilePath();

	static wchar_t* s_pFilePath;
};
