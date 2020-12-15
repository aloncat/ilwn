//⬪MDPN⬪
#pragma once

#include "numset.h"
#include "ttime.h"

#include <core/platform.h>
#include <core/util.h>

#include <memory>
#include <type_traits>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch + Arch::Base
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct Arch final
{
	template <class T, class... Args> static void Execute(Args... args)
	{
		static_assert(std::is_base_of<Arch::Base, T>::value, "Unsupported type");
		std::unique_ptr<T> obj = std::make_unique<T>();
		obj->Execute(args...);
	}

	class LychrelCoincidence;

protected:
	class Base;
};

//----------------------------------------------------------------------------------------------------------------------
class Arch::Base
{
	AML_NONCOPYABLE(Base)

public:
	void Execute();

protected:
	Base() = default;
	bool IsCancelled() const;
	void PrintTimeElapsed(bool showMs = false);

	ThreadTime m_Timer;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch::LychrelCoincidence
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// LychrelCoincidence вычисляет, сколько первичных чисел Лишрел и на какой длине вливаются в потоки проверенных
// ранее чисел Лишрел в том же диапазоне. Эта информация должна помочь при выбре минимально достаточной длины
// числа в наборе отсева, которая обеспечит его максимальную эффективность при поиске отложенных палиндромов

//----------------------------------------------------------------------------------------------------------------------
class Arch::LychrelCoincidence : public Base
{
public:
	// Параметры *Range задают дипапазон проверяемых чисел. Каждый дипазон чисел (все числа одной длины)
	// проверяется отдельно, т.е. собранная статистика очищается при переходе к следующему диапазону
	void Execute(size_t minRange = 3, size_t maxRange = 12);

protected:
	static constexpr size_t MAX_RANGE = 16;
	// Максимальная длина чисел, сохраняемых в наборе m_SieveSet. Для каждого кандидата, чтобы проверить его
	// сходимость, над ним выполняется такое минимальное количество операций RAA, после которого число будет
	// состоять из MAX_CONSEQ_LEN цифр, затем проверяется наличие числа в наборе (добавление при отсутствии)
	static constexpr unsigned MAX_CONSEQ_LEN = 30;

	bool CheckRange(size_t range, unsigned maxStepC);

	NumberSet m_SieveSet;
};

// TODO: REF>>>
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch::CountLychrels
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// CountLychrels вычисляет количество первичных чисел Лишрел и количество всех чисел Лишрел в
// каждом диапазоне чисел до указанного. Также опционально находятся все базовые числа Лишрел

//----------------------------------------------------------------------------------------------------------------------
/*class Arch::CountLychrels : public Base
{
public:
	// Пользовательский коллбэк нахождения числа
	using NumberFoundFn = std::function<void(const Number&)>;

	CountLychrels();

	// Параметр maxRange задаёт конечный диапазон проверяемых чисел (проверка всегда начинается с 1).
	// Если countBase == true, то для каждого диапазона будет выведено количество обнаруженных в нём
	// новых базовых чисел Лишрел и для каждого из чисел будет вызывана указанная функция baseLychFn
	void Execute(size_t maxRange = 12, bool countBase = true, const NumberFoundFn& baseLychFn = nullptr);

protected:
	static constexpr size_t MAX_RANGE = 16;
	// Для эффективности будем хранить числа Лишрел в выделяемых в куче массивах
	// по GROUP_SIZE элементов, а указатели на эти массивы - в std::vector
	static constexpr size_t GROUP_SIZE = 4 * 1024;
	using NumVector = std::vector<FixNumber*>;

	void Clear();
	static void DeleteNumbers(NumVector& v);
	// Возвращает ссылку на i-й элемент в последнем массиве v и увеличивает i на 1.
	// Если i равен GROUP_SIZE, то выделяет новый массив и помещает его в конец v
	static FixNumber& NextNumber(NumVector& v, size_t& i);

	static void RAATillLength(BigNumber& num, size_t len);
	static bool RAATillPalindrome(BigNumber& num, size_t len, unsigned& doneC);

	bool CheckRange(size_t range, unsigned maxStepC, bool countBase);
	bool CheckUnsifted(const NumberFoundFn& baseLychFn);

	NumberSet m_SieveSet;	// Набор отсева чисел Лишрел
	size_t m_SieveNumLen;	// Длина чисел в наборе отсева
	uint64_t m_PrimLychC;	// Кол-во первичных чисел Лишрел в диапазоне
	uint64_t m_AllLychC;	// Количество всех чисел Лишрел в диапазоне

	size_t m_AllBaseC;		// Кол-во всех найденных базовых чисел Лишрел
	NumVector m_AllBase;	// Все найденные базовые числа Лишрел (без сортировки)
	NumVector m_Unsifted;	// Кандидаты в базовые числа Лишрел (в текущем диапазоне)
};*/
