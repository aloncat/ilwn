//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "dbase.h"
#include "eventmgr.h"
#include "mode.h"
#include "stephlp.h"

class DBChunk;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBMode - режим работы программы (базовый класс для режимов работы с БД)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class DBMode : public Mode
{
public:
	// Возвращает путь к базе данных, усечённый слева до не более чем lengthLimit символов
	static std::wstring TruncateDatabasePath(const std::wstring& path, size_t lengthLimit = 60);

	// Выводит через менеджер сообщений указанный путь к базе данных path, усечённый до
	// lengthLimit символов, меняет текст в заголовке окна консоли на "{<PATH>} - MDPN"
	static void PrintDatabasePath(const std::wstring& path, size_t lengthLimit = 60);

protected:
	// Возвращает ожидаемый размер несжатого блока данных в файле на основании количества хранимых в нём
	// палиндромов. Если параметр realSize задан, то обновляет историю значений и возвращает realSize
	static size_t GetDataSize(const DBChunk* chunk, size_t realSize = 0);

protected:
	DataBase m_Data;
	std::unique_ptr<StepHelper> m_Steps;
	std::unique_ptr<EventManager> m_Events;
};
