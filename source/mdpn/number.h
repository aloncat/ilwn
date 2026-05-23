//∙MDPN
#pragma once

#include <core/platform.h>
#include <core/util.h>

#include <algorithm>
#include <string>

class FixNumber;

//----------------------------------------------------------------------------------------------------------------------
template<class T>
class NumberHash
{
public:
	size_t operator ()(const T& num) const { return num.GetHash(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Number - десятичное целое беззнаковое число с произвольным количеством цифр
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class Number
{
	friend class FixNumber;

public:
	Number() = default;
	Number(unsigned num) { Set(num); }
	Number(unsigned long long num) { Set(num); }
	Number(const char* pNum) { Set(pNum); }
	Number(const std::string& num) { Set(num); }
	Number(const Number& that);
	Number(Number&& that);
	~Number();

	void SetZero();
	void Set(unsigned num);
	void Set(unsigned long long num);
	void Set(const char* pNum) { Set(pNum, pNum ? strlen(pNum) : 0); }
	void Set(const std::string& num) { Set(num.c_str(), num.size()); }

	// Возвращает true, если число равно 0
	bool IsZero() const { return m_Length == 1 && !m_DigitA[0]; }
	// Оператор приведения типа к bool возвращает true, если число != 0
	operator bool() const { return m_Length > 1 || m_DigitA[0]; }
	// Возвращает true, если число равно 0 или если содержит более 1 цифры и оканчивается на 0
	bool IsZeroEnded() const { return !m_DigitA[0]; }
	// Возвращает true, если число является палиндромом
	bool IsPalindrome() const { return IsPalindrome(m_DigitA, m_Length); }

	// Возвращает длину числа (количество фифр)
	size_t GetLength() const { return m_Length; }
	// STL-совместимый класс хеш-функции
	using Hasher = NumberHash<Number>;
	// Возвращает 32-битный хеш числа
	unsigned GetHash() const;

	// Возвращает количество родственных чисел в том же диапазоне, значение которых больше. Родственное
	// число - это число, полученное путём изменения одной и более пар симметричных цифр так, чтобы их
	// сумма не изменилась. Например, для числа 234 функция вернёт 4 (числа 333, 432, 531 и 630)
	uint64_t GetKinNumberCount() const;

	// Преобразует число в строку
	std::string AsString() const;
	size_t AsString(char* pBuffer, size_t bufferSize) const;
	// Преобразует число из внутреннего формата в 32-битное целое
	unsigned AsInt() const;
	// Преобразует число из внутреннего формата в 64-битное целое
	uint64_t AsI64() const;

	Number& operator =(unsigned num) { Set(num); return *this; }
	Number& operator =(unsigned long long num) { Set(num); return *this; }
	Number& operator =(const char* pNum) { Set(pNum); return *this; }
	Number& operator =(const std::string& num) { Set(num); return *this; }

	Number& operator =(const Number& rhs);
	Number& operator =(const FixNumber& rhs);
	Number& operator =(Number&& rhs);

	Number operator +(unsigned rhs) const;
	Number operator +(unsigned long long rhs) const;
	Number operator +(const Number& rhs) const;
	Number& operator +=(unsigned rhs);
	Number& operator +=(unsigned long long rhs);
	Number& operator +=(const Number& rhs);

	// Возвращает разность числа и rhs. Если rhs > числа, то будет возвращён 0
	Number operator -(unsigned rhs) const;
	Number operator -(unsigned long long rhs) const;
	Number operator -(const Number& rhs) const;
	// Вычитает rhs от числа. Если rhs > числа, то число станет 0
	Number& operator -=(unsigned rhs);
	Number& operator -=(unsigned long long rhs);
	Number& operator -=(const Number& rhs) { Subtract(m_DigitA, rhs.m_DigitA, rhs.m_Length); return *this; }

	// Увеличивает число на единицу
	Number& operator ++();
	// Уменьшает число на единицу, если число > 0
	Number& operator --();

	bool operator ==(const Number& rhs) const;
	bool operator !=(const Number& rhs) const;
	bool operator <(const Number& rhs) const;
	bool operator <=(const Number& rhs) const;
	bool operator >(const Number& rhs) const;
	bool operator >=(const Number& rhs) const;

protected:
	class Allocator;

	void Allocate(uint32_t newLength, bool copy = false);
	void CopyDigits(void* pTo, const void* pFrom);

	void Set(const char* pStr, size_t size);
	static uint32_t Decode(uint8_t* digitA, unsigned num);
	static uint32_t Decode(uint8_t* digitA, unsigned long long num);

	void Add(const Number* pLhs, const Number& rhs);
	template<class T> void Add(const Number* pLhs, const T& rhs);
	void Add(const uint8_t* lhsA, const uint8_t* rhsA, size_t rhsLen);

	void Subtract(const Number* pLhs, const Number& rhs);
	template<class T> void Subtract(const Number* pLhs, const T& rhs);
	void Subtract(const uint8_t* lhsA, const uint8_t* rhsA, size_t rhsLen);

	bool IsGreater(const char* digitA, size_t len) const;
	static bool IsPalindrome(const uint8_t* digitA, size_t len);
	static void OnError(const char* pMsg = nullptr);

	static uint8_t s_ZeroDigits[sizeof(size_t)];

	uint32_t m_Length = 1;				// Длина числа (количество цифр)
	uint32_t m_MaxLength = 0;			// Размер массива m_DigitA (макс. допустимое для него кол-во цифр)
	uint8_t* m_DigitA = s_ZeroDigits;	// Цифры числа (от 0 до 9), расположены от младшего разряда к старшему
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FixNumber - минимальная версия класса числа с фиксированной максимальной длиной
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class FixNumber
{
	friend class Number;

public:
	FixNumber() : m_LengthAnd1stDigit(Z_DIGIT) {}
	FixNumber(const Number& that) : m_LengthAnd1stDigit(Z_DIGIT) { *this = that; }
	FixNumber(const char* pNum) : m_LengthAnd1stDigit(Z_DIGIT) { *this = pNum; }

	void SetZero() { m_LengthAnd1stDigit = Z_DIGIT; }
	bool IsZero() const { return m_LengthAnd1stDigit == Z_DIGIT; }
	operator bool() const { return m_LengthAnd1stDigit != Z_DIGIT; }

	size_t GetLength() const { return m_Length; }
	using Hasher = NumberHash<FixNumber>;
	unsigned GetHash() const;

	FixNumber& operator =(const Number& rhs);
	FixNumber& operator =(const char* pNum);

	bool operator ==(const FixNumber& rhs) const;
	bool operator !=(const FixNumber& rhs) const;
	bool operator ==(const Number& rhs) const;
	bool operator !=(const Number& rhs) const;

	friend bool operator ==(const Number& lhs, const FixNumber& rhs) { return rhs == lhs; }
	friend bool operator !=(const Number& lhs, const FixNumber& rhs) { return rhs != lhs; }

	bool operator <(const FixNumber& rhs) const;
	bool operator <=(const FixNumber& rhs) const;
	bool operator >(const FixNumber& rhs) const;
	bool operator >=(const FixNumber& rhs) const;

protected:
	static constexpr uint16_t Z_DIGIT = AML_TO_LE16(1);
	static constexpr size_t OBJ_SIZE = 4 * sizeof(uint32_t);	// 16 байт на весь объект
	static constexpr size_t MAX_LENGTH = (OBJ_SIZE - 1) * 2;	// Макс. 30 цифр (2 цифры на байт)

	size_t Unpack(uint8_t* digitA) const;

	union {
		uint8_t m_Length;
		uint8_t m_DigitA[OBJ_SIZE];
		uint16_t m_LengthAnd1stDigit;
		// NB: желательно, чтобы m_DigitA был выровнен по границе 4 байт, потому что в
		// операторах == и != для производительности мы сравниваем словами по 4/8 байт
		uint32_t m_AlignDummy;
	};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BigNumber - расширенная версия Number
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class BigNumber : public Number
{
public:
	BigNumber() = default;
	BigNumber(unsigned num) { Set(num); }
	BigNumber(unsigned long long num) { Set(num); }
	BigNumber(const char* pNum) { Set(pNum); }
	BigNumber(const std::string& num) { Set(num); }
	BigNumber(const BigNumber& that);
	BigNumber(const Number& that);
	BigNumber(BigNumber&& that);
	~BigNumber();

	// Выделяет память, достаточную для хранения числа, состоящего из maxLength цифр.
	// Также будет выделен временный буфер для RAATillPalindrome (такого же размера)
	void Reserve(size_t maxLength);

	// Если число после применения ReverseAndAdd даёт такой же результат, как какое-нибудь число такой же длины, но
	// меньшее данного, то функция увеличивает число до ближайшего следующего, дающего уникальный результат. Если при
	// применении операции RAA к числу, никакое другое меньшее не даёт такой же результат, то число изменено не будет
	void SkipRAADups();

	// Добавляет к числу обратное ему число (Reverse-And-Add) stepС раз
	void ReverseAndAdd(unsigned stepC = 1) { if (m_MaxLength) RAA(stepC, false); }
	// Выполняет над числом операцию Reverse-And-Add до тех пор, пока оно не станет палиндромом, или
	// не будет выполнено stepC операций. Если палиндром получен, то функция вернёт true, а doneC будет
	// содержать количество реально выполненных операций. Если будет достигнут лимит операций stepC
	// без получения палиндрома, то функция вернёт false и doneC будет равен значению stepC
	bool RAATillPalindrome(unsigned stepC, unsigned& doneC);
	// Выполняет над числом операцию Reverse-And-Add до тех пор, пока оно не станет палиндромом, или не
	// достигнет длины length цифр. Если палиндром получен, то функция вернёт true. Если при достижении
	// указанной длины число так и не станет палиндромом, то функция вернёт false. В любом из случаев
	// параметр doneC будет содержать количество выполненных операций
	bool RAATillLength(size_t length, unsigned& doneC);

	// Возвращает true, если обратное число меньше. Для палиндромов и числа 0 функция возвращает false,
	// однако для многозначных чисел, заканчивающихся на 0, функция возвращает true. Например, функция
	// вернёт false для чисел: 0, 5, 11, 272, 123, 1422; и вернёт true для чисел: 10, 251, 370, 2241
	bool IsReversedLower() const;

	// Возвращает обратное число
	BigNumber GetReversed() const;

	BigNumber& operator =(unsigned num) { Set(num); return *this; }
	BigNumber& operator =(unsigned long long num) { Set(num); return *this; }
	BigNumber& operator =(const char* pNum) { Set(pNum); return *this; }
	BigNumber& operator =(const std::string& num) { Set(num); return *this; }

	BigNumber& operator =(const BigNumber& rhs) { return (BigNumber&) Number::operator =(rhs); }
	BigNumber& operator =(const Number& rhs) { return (BigNumber&) Number::operator =(rhs); }
	BigNumber& operator =(const FixNumber& rhs) { return (BigNumber&) Number::operator =(rhs); }
	BigNumber& operator =(BigNumber&& rhs) { return (BigNumber&) Number::operator =(std::move(rhs)); }
	BigNumber& operator =(Number&& rhs) { return (BigNumber&) Number::operator =(std::move(rhs)); }

	BigNumber operator +(unsigned rhs) const;
	BigNumber operator +(unsigned long long rhs) const;
	BigNumber operator +(const Number& rhs) const;
	BigNumber& operator +=(unsigned rhs) { return (BigNumber&) Number::operator +=(rhs); }
	BigNumber& operator +=(unsigned long long rhs) { return (BigNumber&) Number::operator +=(rhs); }
	BigNumber& operator +=(const Number& rhs) { return (BigNumber&) Number::operator +=(rhs); }

	// Возвращает разность числа и rhs. Если rhs > числа, то будет возвращён 0
	BigNumber operator -(unsigned rhs) const;
	BigNumber operator -(unsigned long long rhs) const;
	BigNumber operator -(const Number& rhs) const;
	// Вычитает rhs от числа. Если rhs > числа, то число станет 0
	BigNumber& operator -=(unsigned rhs) { return (BigNumber&) Number::operator -=(rhs); }
	BigNumber& operator -=(unsigned long long rhs) { return (BigNumber&) Number::operator -=(rhs); }
	BigNumber& operator -=(const Number& rhs) { return (BigNumber&) Number::operator -=(rhs); }

	// Увеличивает число на единицу
	BigNumber& operator ++() { return (BigNumber&) Number::operator ++(); }
	// Уменьшает число на единицу, если число > 0
	BigNumber& operator --() { return (BigNumber&) Number::operator --(); }

protected:
	// Размер локального буфера для RAATillPalindrome (выделяется на стеке)
	static constexpr size_t LOCAL_RAA_BUFFER_SIZE = 640;
	// Массив масок для операции RAA
	static const size_t raaMask[8];

	uint8_t* AllocateRAABuffer(size_t maxLength);

	// Если stopOnPalindrome равен false, то функция выполнит ровно stepC операций RAA и вернёт 0. Если
	// stopOnPalindrome равен true, то функция выполнит не более stepC шагов до нахождения палиндрома.
	// Если палиндром был получен, то функция вернёт количество выполненных шагов, иначе вернёт 0
	unsigned RAA(unsigned stepC, bool stopOnPalindrome);

	uint8_t* m_pRAABuffer = nullptr;	// Временный буфер для функции RAA
	size_t m_RAABufferSize;				// Размер временного буфера m_pRAABuffer
};
