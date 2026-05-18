//∙MDPN
#pragma once

#include "const.h"
#include "number.h"

#include <core/util.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   StepHelper - вспомогательный класс для проверки условий, связанных с шагом
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class StepHelper final
{
	AML_NONCOPYABLE(StepHelper)

public:
	// Инициализирует класс. Параметр startRange задаёт диапазон, с которого начат поиск. Например, при
	// запуске программы с пустой БД и при продолжении поиска с этой же базой startRange будет равен 1.
	// При запуске с отдельной БД для поиска в диапазоне 21-значных чисел startRange будет равен 21
	explicit StepHelper(size_t startRange);

	// Возвращает true, если палиндром длины digitC для шага step должен быть сохранён в БД
	bool IsSaveable(size_t digitC, unsigned step) const
	{
		return digitC <= s_MaxLengthA[(step <= Const::MAX_STEP) ? step : Const::MAX_STEP];
	}

	// Возвращает true, если палиндром для указанного шага может являться "новым"
	// при значении startRange, с которым мы начали/возобновили поиск в текущей БД
	bool IsNew(unsigned step) const
	{
		return m_StartRange <= s_FoundRangeA[(step <= Const::MAX_STEP) ? step : Const::MAX_STEP];
	}

	// Возвращает максимальную глубину поиска для палиндромов указанной длины
	unsigned GetSearchLimit(const Number& num) const { return GetSearchLimit(num.GetLength()); }
	unsigned GetSearchLimit(size_t digitC) const;

	// Возвращает шаг, начиная с которого палиндромы указанной длины должны сохраняться в БД
	unsigned GetMinSaveable(const Number& num) const { return GetMinSaveable(num.GetLength()); }
	unsigned GetMinSaveable(size_t digitC) const;

	// Возвращает наибольший известный шаг среди всех диапазонов ниже диапазона числа num
	static unsigned GetHighest(const Number& num);

private:
	static void Init();
	static void InitMaxLengths();
	static void InitFoundRanges();
	static void InitMinSaveables();

	const size_t m_StartRange = 1;
	static bool s_RangeWarningShown;

	static uint8_t s_MaxLengthA[Const::MAX_STEP + 1];
	static uint8_t s_FoundRangeA[Const::MAX_STEP + 1];
	static uint16_t s_MinSaveableA[Const::MAX_DIGIT_C + 1];
};
