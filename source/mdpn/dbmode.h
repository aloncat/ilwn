//∙MDPN
#pragma once

#include "dbase.h"
#include "eventmgr.h"
#include "mode.h"
#include "stephlp.h"

class DBChunk;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBMode - режим работы программы (базовый класс для режимов работы с БД)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBMode : public Mode
{
public:
	// Возвращает ожидаемый размер несжатого блока данных в файле на основании количества хранимых в нём
	// палиндромов. Если параметр realSize задан, то обновляет историю значений и возвращает realSize
	static size_t GetDataSize(const DBChunk* pChunk, size_t realSize = 0);

	// Выводит через менеджер сообщений указанный путь к базе данных path, усечённый до
	// lengthLimit символов, меняет текст в заголовке окна консоли на "{<PATH>} - MDPN"
	static void PrintDataBasePath(const std::wstring& path, size_t lengthLimit = 60);

protected:
	DataBase m_Data;
	std::unique_ptr<StepHelper> m_Steps;
	std::unique_ptr<EventManager> m_Events;
};
