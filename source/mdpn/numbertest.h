//⬪MDPN⬪
#pragma once

#include "number.h"
#include "test.h"

#include <core/platform.h>
#include <core/randgen.h>

#include <functional>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Validity.Number - тест корректности работы класса Number
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class TestNumber : public Test
{
public:
	static std::string GetId() { return "Test.Validity.Number"; }

	virtual bool Execute() override;

protected:
	virtual std::string GetPrintedName() const override { return "Number"; }
	virtual std::string GetPrintedHeader() const override;

	static unsigned GetHash(const Number& num);
	static bool Equal(const Number& num, unsigned value);

	// Вызывает функцию fn для loopC случайно сгенерированных чисел длины N, где N принимает значения от
	// minLen до maxLen включительно. Если fn вернёт false, то процесс прервётся и функция вернёт false
	bool ForRandomNumbers(size_t minLen, size_t maxLen, size_t loopC, const std::function<bool(char*, size_t)>& fn);
	// Возвращает случайно сгенерированное число. В отличие от RandGen::UInt(), эта
	// функция генерирует числа, равномерно распределённые по количеству цифр в них
	unsigned Rand();

	math::RandGen m_Rg;

private:
	bool TestCtorSetAs();
	bool TestComparison();
	bool TestCopyMove();

	bool TestIncDec();
	bool TestAddition();
	bool TestSubtraction();

	bool TestGetHash();
	bool TestIsPalindrome();
	bool TestGetKinNumberC();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Validity.FixNumber - тест корректности работы класса FixNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class TestFixNumber : public TestNumber
{
public:
	static std::string GetId() { return "Test.Validity.FixNumber"; }
	static std::string GetPrerequisites() { return "Test.Validity.Number"; }

	virtual bool Execute() override;

protected:
	virtual std::string GetPrintedName() const override { return "FixNumber"; }

private:
	bool TestCtorSet();
	bool TestComparison();
	bool TestGetHash();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Validity.BigNumber - тест корректности работы класса BigNumber (основная функциональность)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class TestBigNumber : public TestNumber
{
public:
	static std::string GetId() { return "Test.Validity.BigNumber"; }
	static std::string GetPrerequisites() { return "Test.Validity.Number"; }

	virtual bool Execute() override;

protected:
	virtual std::string GetPrintedName() const override { return "BigNumber"; }

private:
	bool TestCtorCopyMove();
	bool TestIncDec();
	bool TestAddSub();
	bool TestReversed();
	bool TestReverseAndAdd();
	bool TestRAATillPalindrome();
	bool TestRAATillLength();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Validity.BigNumber.SkipRAADups - тест корректности работы функции BigNumber::SkipRAADups
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class TestBigNumberSkipRAADups : public TestBigNumber
{
public:
	static std::string GetId() { return "Test.Validity.BigNumber.SkipRAADups"; }
	static std::string GetPrerequisites() { return "Test.Validity.BigNumber"; }

	virtual bool Execute() override;

protected:
	class NumSet;

	virtual std::string GetPrintedName() const override { return "BigNumber::SkipRAADups"; }
	bool OnError(unsigned errorNum = 0);

private:
	static constexpr size_t MAIN_MAX_RANGE = 6;
	static constexpr size_t HIGH_MAX_RANGE = 10;

	bool TestMain();
	bool TestHigh();
};

//----------------------------------------------------------------------------------------------------------------------
class TestBigNumberSkipRAADups::NumSet
{
public:
	NumSet(size_t maxDigitC);
	~NumSet();

	void Clear(size_t digitC);
	// Добавляет число в набор. Если число было добавлено, функция вернёт
	// true. Если такое число уже было в наборе, то функция вернёт false
	bool Insert(const Number& num);

private:
	size_t m_Size = 0;			// Размер массива m_NumA в байтах
	uint8_t* m_NumA = nullptr;	// Числа набора: 1 бит - 1 число, отсчёт от 0
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Speed.P196RAA - измерение времени работы цикла Reverse-And-Add до достижения числом 196 длины в 1M цифр
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SpeedTestP196RAA : public Test
{
public:
	static std::string GetId() { return "Test.Speed.P196RAA"; }

	virtual bool Execute() override;

protected:
	virtual std::string GetPrintedName() const override { return "P196RAA-1M"; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Speed.RAA - измерение скорости работы цикла Reverse-And-Add
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class SpeedTestRAA : public Test
{
public:
	static std::string GetId() { return "Test.Speed.RAA"; }

	virtual bool Execute() override;

protected:
	static constexpr unsigned CONSEQ_LEN = 26;
	static constexpr unsigned SEARCH_DEPTH = 305;

	virtual std::string GetPrintedHeader() const override;

	size_t RunTestLoop();
	static void GetCounter(uint64_t& counter);
	static void GetCounterAndFrequency(uint64_t& counter, uint64_t& frequency);

	std::vector<Number> m_Numbers;
};
