//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "options.h"

#include <core/file.h>
#include <core/platform.h>
#include <core/util.h>

#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SearchBase - базовый класс (поиск коэффициентов диофантова уравнения вида p.m.n)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SearchBase
{
	AML_NONCOPYABLE(SearchBase)

public:
	static constexpr int MAX_POWER = 20;
	static constexpr int MAX_FACTOR_COUNT = 160;

	SearchBase() = default;
	virtual ~SearchBase();

	// Выполняет поиск. Если произойдёт ошибка, то функция вернёт false.
	// В этом случае сообщение об ошибке будет выведено в окно консоли
	bool Run(int power, int leftCount, int rightCount, const Options& options);

	// Выводит справку по опциям
	static void PrintOptionsHelp();

	// Инициализирует опции
	static bool InitOptions(Options& options);

protected:
	struct EquationInfo {
		int power = 0;			// Степень уравнения (от 1 до MAX_POWER)
		int leftCount = 0;		// Количество коэффициентов в левой части
		int rightCount = 0;		// Количество коэффициентов в правой части
	};

	// Обновляет значение времени работы
	void UpdateRunningTime();
	// Возвращает время работы в секундах
	float GetRunningTime() const;

	// Возвращает true, если алгоритм подходит для указаанных параметров уравнения. Если параметр
	// allowAll равен true, то значит, что разрешены любые алгоритмы, в том числе экспериментальные
	static bool IsSuitable(int power, int leftCount, int rightCount, bool allowAll) { return false; }

	// Возвращает строку с дополнительной информацией об алгоритме. Используется
	// в классах наследниках, реализующих специализированные алгоритмы поиска
	virtual std::wstring GetAdditionalInfo() const;

	// Выполняет поиск решений. Вектор startFactors
	// содержит стартовые значения первых коэффициентов
	virtual void Search(const Options& options, const std::vector<unsigned>& startFactors) = 0;

	// Обновляет заголовок окна консоли
	virtual void UpdateConsoleTitle() = 0;

private:
	// Открывает (создаёт) файл журнала
	bool OpenLogFile();

	// Возвращает список стартовых значений первых
	// коэффициентов, полученный из командной строки
	static std::vector<unsigned> GetStartFactors();

protected:
	EquationInfo m_Info;		// Параметры уравнения
	util::BinaryFile m_Log;		// Файл журнала (для вывода результатов)

private:
	uint32_t m_StartTick = 0;	// Тик начала работы программы (обновляется)
	unsigned m_RunningTime = 0;	// Время работы программы в полных секундах
};
