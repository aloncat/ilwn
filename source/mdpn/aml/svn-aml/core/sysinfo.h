#pragma once

#include "../../core/platform.h"
#include "singleton.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class SystemInfo
{
	AML_SIMPLE_SINGLETON(SystemInfo)

public:
	// Возвращает количество логических процессоров в системе. Значение
	// равно макс. возможному количеству одновременно активных потоков.
	unsigned			GetLogicalCoreC() const { return m_LogicalCoreC; }
	// Возвращает дату и время запуска (UTC) в формате DateTime. Инициализируется
	// значением текущего системного времени ОС в момент инициализации SystemInfo.
	uint64_t			GetStartingDateTime() const { return m_StartingDateTime; }

	// Возвращает количество секунд, прошедших с момента старта приложения, а точнее с момента
	// инициализации SystemInfo. На Windows Server 2003 и Windows XP это значение ограничено
	// 4'294'967 секундами (примерно 49.71 суток). При превышении этого порога отсчет снова
	// начинается с 0. На более новых ОС семейства Windows такого ограничения нет.
	unsigned			GetAppUptime() const;

	// Возвращает полный путь к исполнимому файлу приложения.
	static std::wstring	GetAppExePath();

	// Возвращает количество секунд, прошедших с момента старта операционной системы. На Windows Server
	// 2003 и Windows XP эта функция возвращает корректные значения только до момента, пока время работы
	// приложения не превысит 49.71 суток. На более новых ОС семейства Windows такого ограничения нет.
	static unsigned		GetOSUptime();

private:
	unsigned	m_LogicalCoreC;
	uint64_t	m_StartingDateTime;
	unsigned	m_FirstTick;
};

} // namespace util
