//⬪MDPN⬪
#include "pch.h"
#include "stephlp.h"

#include <core/auxutil.h>

#include <algorithm>

uint8_t StepHelper::s_MaxLengthA[Const::MAX_STEP + 1];
uint8_t StepHelper::s_FoundRangeA[Const::MAX_STEP + 1];

//----------------------------------------------------------------------------------------------------------------------
StepHelper::StepHelper(size_t startRange)
	: m_StartRange(startRange ? startRange : 1)
{
	static bool isInitialized = Init();
}

//----------------------------------------------------------------------------------------------------------------------
unsigned StepHelper::GetSearchLimit(size_t digitC) const
{
	constexpr size_t KNOWN_DIGIT_C = 21;

	// Выведем предупреждение о необходимости обновления параметров класса
	// при попытке работы с диапазонами, старше доверенных (> KNOWN_DIGIT_C)
	if (digitC > KNOWN_DIGIT_C && !m_RangeWarningShown)
	{
		m_RangeWarningShown = true;
		aux::Printf("#12WARNING: #7StepHelper class needs to be updated"
			" to process #15#%u#7-digit numbers\n", digitC);
	}

	if (digitC <= KNOWN_DIGIT_C)
	{
		static const unsigned limitA[KNOWN_DIGIT_C + 1] = { 0,
			100, 100, 100, 105, 120, 135, 150, 165, 180, 190,	//  1 - 10
			205, 220, 235, 250, 265, 280, 290, 305, 320, 335,	// 11 - 20
			350 };												// 21
		return limitA[digitC];
	}

	// Используем линейную регрессию (коэффициенты получены путём расчётов в Excel на
	// имеющихся данных 20 проверенных диапазонов), чтобы спрогнозировать достаточную
	// глубину поиска для диапазонов, проверка которых ещё не завершена
	const float depth = 14.211f * digitC - 16.968f + 6.f * 10.58f;
	const unsigned limit = static_cast<unsigned>(depth + 2.05f);
	// Округляем вверх до кратного 5
	return ((limit + 4) / 5) * 5;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned StepHelper::GetMinSaveable(size_t digitC) const
{
	unsigned step = 1;
	digitC = std::min(digitC, Const::MAX_DIGIT_C);
	while (step < Const::MAX_STEP && digitC > s_MaxLengthA[step])
		++step;
	return step;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned StepHelper::GetHighest(const Number& num)
{
	constexpr size_t KNOWN_DIGIT_C = 21;
	// Каждый элемент массива - это значение наибольшего известного шага среди
	// всех диапазонов ниже того, который соответствует индексу этого элемента
	static const unsigned stepA[KNOWN_DIGIT_C + 1] = { 0, 0, 2, 24, 24, 24, 55,
		64, 96, 96, 98, 109, 149, 149, 188, 188, 201, 201, 236, 236, 261, 261 };
	return stepA[std::min(num.GetLength(), KNOWN_DIGIT_C)];
}

//----------------------------------------------------------------------------------------------------------------------
bool StepHelper::Init()
{
	InitMaxLengths();
	InitFoundRanges();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void StepHelper::InitMaxLengths()
{
	AML_FILLA(s_MaxLengthA, 0xff, Const::MAX_STEP + 1);
	s_MaxLengthA[0] = 0;

	// Мы не можем сохранять в БД все найденные отложенные палиндромы, так как их очень и очень много.
	// И так как бОльшую часть всех палиндромов составляют палиндромы для низких шагов, то именно их
	// мы и исключим из базы данных. Но оставим по меньшей мере по 10 миллиардов первых таких чисел
	// для каждого из шагов. Начиная с шага 50, мы будем сохранять абсолютно все найденные числа
	for (unsigned step = 1; step <= 9; ++step)
		s_MaxLengthA[step] = 12;
	for (unsigned step = 10; step <= 14; ++step)
		s_MaxLengthA[step] = 13;
	for (unsigned step = 15; step <= 34; ++step)
		s_MaxLengthA[step] = 14;
	for (unsigned step = 35; step <= 39; ++step)
		s_MaxLengthA[step] = 15;
	for (unsigned step = 40; step <= 44; ++step)
		s_MaxLengthA[step] = 17;
	for (unsigned step = 45; step <= 49; ++step)
		s_MaxLengthA[step] = 19;
}

//----------------------------------------------------------------------------------------------------------------------
void StepHelper::InitFoundRanges()
{
	struct R {
		unsigned from, to;
		R(unsigned f, unsigned t = 0) : from(f), to(t ? t : f) {}
		// Отмечает каждый шаг из указанного множества ranges как "может быть обнаружен в диапазоне
		// digitC". После окончания инициализации массив s_FoundRangeA будет содержать для каждого
		// шага значение диапазона, в котором будет найден соответствующий наименьший отложенный
		// палиндром или ~0, если наименьший отложенный палиндром для этого шага ещё неизвестен
		R& operator <<(const R& r)
		{
			for (unsigned i = r.from; i <= r.to; ++i)
				s_FoundRangeA[i] = from & 0xff;
			return *this;
		}
	};

	s_FoundRangeA[0] = 0;

	R(1) << R(1, 2);
	R(2) << R(3, 24);
	R(3) << 5 << R(7, 23);
	R(4) << 9 << R(12, 13) << 18 << R(20, 21);
	R(5) << R(25, 55);
	R(6) << R(34, 36) << R(41, 46) << R(48, 51) << R(56, 64);
	R(7) << R(41, 44) << R(48, 49) << 56 << R(61, 62) << R(65, 96);
	R(8) << R(71, 74) << 76 << 81 << R(83, 95);
	R(9) << R(84, 93) << R(97, 98);
	R(10) << R(99, 109);
	R(11) << 102 << R(106, 107) << R(110, 149);
	R(12) << R(120, 121) << R(124, 129) << R(138, 143);
	R(13) << R(125, 128) << R(138, 141) << R(150, 188);
	R(14) << 138 << R(150, 182);
	R(15) << R(150, 180) << R(189, 201);
	R(16) << R(160, 162) << 168 << R(175, 177) << R(193, 197);
	R(17) << 160 << R(202, 236);
	R(18) << R(202, 205) << R(211, 228);
	R(19) << 217 << R(223, 226) << R(237, 261);
	R(20) << 223 << R(237, 257);
	R(21) << R(237, 252);

	// Ненайденные шаги (обновятся при обнаружении нового
	// шага в диапазоне 21-значных чисел или старше)
	R(~0u) << R(237, 251) << R(262, Const::MAX_STEP);
}
