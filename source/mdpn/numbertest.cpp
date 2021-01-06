//⬪MDPN⬪
#include "pch.h"
#include "numbertest.h"

#include "ttime.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/array.h>
#include <core/exception.h>
#include <core/fasthash.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>

#include <algorithm>
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::Execute()
{
	PrintHeader();

	if (!IsCancelled() && !TestCtorSetAs())
		return false;
	if (!IsCancelled() && !TestComparison())
		return false;
	if (!IsCancelled() && !TestCopyMove())
		return false;

	if (!IsCancelled() && !TestIncDec())
		return false;
	if (!IsCancelled() && !TestAddition())
		return false;
	if (!IsCancelled() && !TestSubtraction())
		return false;

	if (!IsCancelled() && !TestGetHash())
		return false;
	if (!IsCancelled() && !TestIsPalindrome())
		return false;
	if (!IsCancelled() && !TestGetKinNumberC())
		return false;

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
std::string TestNumber::GetPrintedHeader() const
{
	return util::Format("Testing validity of #9#%s#7", GetPrintedName().c_str());
}

//----------------------------------------------------------------------------------------------------------------------
unsigned TestNumber::GetHash(const Number& num)
{
	auto s = num.AsString();
	const size_t len = s.size();
	util::SmartArray<uint8_t> buffer(len);

	const char* p = s.c_str() + len;
	for (size_t i = 0; i < len; ++i)
		buffer[i] = *(--p) - '0';

	return hash::GetFastHash(buffer, len);
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::Equal(const Number& num, unsigned value)
{
	char buffer[12], numBuffer[12];
	const size_t len = sprintf_s(buffer, "%u", value);
	// Помимо проверки цифр числа, каждый раз также проверяем корректность
	// функции GetLength и конструктора по умолчанию, инициализирующего число нолём
	return num.AsInt() == value && num.AsI64() == value && num.GetLength() == len &&
		num.AsString(numBuffer, 12) == len && !util::StrCmp(buffer, numBuffer) && Number().IsZero();
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::ForRandomNumbers(size_t minLen, size_t maxLen, size_t loopC,
	const std::function<bool(char*, size_t)>& fn)
{
	if (minLen > maxLen || !loopC || !fn)
		return false;

	util::SmartArray<char> buffer(maxLen + 1);
	for (size_t len = minLen; len <= maxLen; ++len)
	{
		buffer[len] = 0;
		for (size_t i = 0; i < loopC; ++i)
		{
			buffer[0] = '1' + m_Rg.UInt() % 9;
			for (size_t j = 1; j < len; ++j)
				buffer[j] = '0' + m_Rg.UInt() % 10;
			if (!fn(buffer, len))
				return false;
		}
	}

	for (size_t i = 0; i < 3 * loopC; ++i)
	{
		size_t len = 1 + m_Rg.UInt() % maxLen;
		buffer[len] = 0;

		buffer[0] = '1' + m_Rg.UInt() % 9;
		for (size_t j = 1; j < len; ++j)
			buffer[j] = '0' + m_Rg.UInt() % 10;
		if (!fn(buffer, len))
			return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE unsigned TestNumber::Rand()
{
	static const unsigned rangeA[] = { 10, 90, 900, 9000, 90000, 900000, 9000000, 90000000, 900000000, 3294967296 };
	static const unsigned offsetA[] = { 0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

	const unsigned num = m_Rg.UInt();
	const unsigned idx = m_Rg.UInt(10);
	return offsetA[idx] + num % rangeA[idx];
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestCtorSetAs()
{
	// Тестируем конструкторы (кроме конструкторов копирования/перемещения),
	// а также функции Set*, IsZero*, GetLength, As* и оператор bool()

	auto CheckZero = [](const Number& num) {
		return !num && num.IsZero() && num.IsZeroEnded() && Equal(num, 0);
	};
	auto CheckNum = [](const Number& num, unsigned n) {
		return Equal(num, n) && !num == !n && num.IsZero() == !n && num.IsZeroEnded() == !(n % 10);
	};

	auto fn = [&](const char* p, unsigned n) {
		Number num;
		if (!CheckZero(num))
			return false;
		num.Set(p);
		std::string s(p);
		Number n1(n), n2(static_cast<uint64_t>(n)), n3(p), n4(s);
		if (!CheckNum(num, n) || !CheckNum(n1, n) || !CheckNum(n2, n) || !CheckNum(n3, n) || !CheckNum(n4, n))
			return false;
		num.Set(0u);
		n1.SetZero();
		n2.SetZero();
		n3.Set(0ull);
		if (!CheckZero(num) || !CheckZero(n1) || !CheckZero(n2) || !CheckZero(n3))
			return false;
		n1.Set(p);
		n2.Set(s);
		n3.Set(n);
		num.Set(static_cast<uint64_t>(n));
		return CheckNum(num, n) && CheckNum(n1, n) && CheckNum(n2, n) && CheckNum(n3, n);
	};

	constexpr size_t NUM_C = 9;
	static const char* set1A[NUM_C] = { "0", "1", "00", "03", "50", "1000", "0000178", "987654321", "4294967295" };
	static const unsigned set2A[NUM_C] = { 0, 1, 0, 3, 50, 1000, 178, 987654321, 4294967295 };

	for (size_t i = 0; i < NUM_C; ++i)
	{
		if (!fn(set1A[i], set2A[i]))
			return OnError(1);
	}

	char buffer[24];
	Number num, n1, n2;
	for (int i = 0; i < 1000; ++i)
	{
		unsigned n = Rand();
		sprintf_s(buffer, "%u", n);

		if ((i & 0x44) == 4)
			num.SetZero();
		num.Set(buffer);
		if ((i & 0x81) == 1)
			n1.SetZero();
		n1.Set(n);
		if ((i & 0x82) == 2)
			n2.SetZero();
		n2.Set(static_cast<uint64_t>(n));

		if (!fn(buffer, n) || !Equal(num, n) || num.IsZeroEnded() != !(n % 10) || !Equal(n1, n) || !Equal(n2, n))
			return OnError(2);
	}

	auto CheckAsString = [&](char* p, size_t len)
	{
		num.Set(p);
		char buffer[24];
		return num.AsString(buffer, 24) == len && !util::StrCmp(num.AsString(), buffer);
	};
	if (!ForRandomNumbers(1, 23, 100, CheckAsString))
		return OnError(3);

	unsigned long long nn;
	for (int i = 0; i < 500; ++i)
	{
		nn = (static_cast<uint64_t>(Rand()) << 32) | m_Rg.UInt();
		const size_t len = sprintf_s(buffer, "%llu", nn);
		num.Set(nn);

		if (num.AsI64() != nn || util::StrCmp(num.AsString(), buffer) || num.GetLength() != len || !Number().IsZero())
			return OnError(4);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestComparison()
{
	// Тестируем операторы сравнения

	auto fn = [](const Number& n, const Number& m, unsigned i, unsigned j) {
		return (n == m) == (i == j) &&
			(n != m) == (i != j) &&
			(n < m) == (i < j) &&
			(n <= m) == (i <= j) &&
			(n > m) == (i > j) &&
			(n >= m) == (i >= j);
	};

	for (unsigned i = 0; i <= 150; ++i)
	{
		Number n(i), m;
		for (unsigned j = 0; j <= 150; ++j)
		{
			m.Set(j);
			if (!fn(n, m, i, j))
				return OnError(5);
		}
	}

	Number n, m;
	for (int k = 0; k < 1000; ++k)
	{
		unsigned i = Rand();
		unsigned j = (k & 7) ? Rand() : i;
		n.Set(i);
		m.Set(j);
		if (!fn(n, m, i, j))
			return OnError(6);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestCopyMove()
{
	// Тестируем конструкторы и операторы копирования/перемещения

	Number num, n0, n3;
	Number n1(num), n2(std::move(n0));
	if (num || n1 || n2 || !Equal(num, 0) || !Equal(n1, 0) || !Equal(n2, 0))
		return OnError(7);

	for (int i = 0; i < 500; ++i)
	{
		num.Set(Rand());
		Number numCopy(num);
		if (num != numCopy || num != Number(std::move(numCopy)))
			return OnError(8);
	}

	unsigned n;
	num = num = n = Rand();
	if (!num != !n || !Equal(num, n))
		return OnError(9);

	num = n = Rand();
	num = std::move(num);
	if (!num != !n || !Equal(num, n))
		return OnError(10);

	for (int i = 0; i < 500; ++i)
	{
		num.Set(Rand());
		n1 = num;
		Number numCopy(num);
		n2 = std::move(numCopy);
		if (num != n1 || num != n2)
			return OnError(11);
	}

	char buffer[12];
	for (int i = 0; i < 500; ++i)
	{
		num = n = Rand();
		sprintf_s(buffer, "%u", n);
		n1 = static_cast<uint64_t>(n);
		n2 = buffer;
		n3 = std::string(buffer);
		if (!Equal(num, n) || !Equal(n1, n) || !Equal(n2, n) || !Equal(n3, n))
			return OnError(12);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestIncDec()
{
	// Тестируем операторы ++, -- и оператор bool()

	unsigned n;
	Number num;
	for (n = 0; n < 105; ++n, ++num)
	{
		if (!num != !n || !Equal(num, n))
			return OnError(13);
	}

	num = n = 125;
	for (int i = 0; i < 130; ++i)
	{
		--num;
		n && --n;
		if (!num != !n || !Equal(num, n))
			return OnError(14);
	}

	if (!num.IsZero() || !num.IsZeroEnded())
		return OnError(15);

	for (int i = 0; i < 500; ++i)
	{
		num = n = Rand();
		if (!Equal(++num, n + 1) || !Equal(--num, n))
			return OnError(16);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestAddition()
{
	// Тестируем операторы сложения

	Number num, n0, n1, n2, n3;
	for (unsigned n = 0; n <= 100; ++n)
	{
		n2 = n1 = num = n;
		if (!Equal(num + 0u, n) || !Equal(num + 0ull, n) || !Equal(num + n3, n) ||
			!Equal(num += 0u, n) || !Equal(n1 += 0ull, n) || !Equal(n2 += n3, n))
			return OnError(17);
	}

	for (unsigned n = 0; n <= 100; ++n)
	{
		n1 = n;
		n3 = n2 = n0;
		num.SetZero();
		uint64_t nn = n;
		if (!Equal(num + n, n) || !Equal(num + nn, n) || !Equal(num + n1, n) ||
			!Equal(num += n, n) || !Equal(n2 += nn, n) || !Equal(n3 += n1, n) ||
			!Equal(Number() + n, n) || !Equal(Number() + nn, n) || !Equal(Number() + n1, n) ||
			!Equal(Number() += n, n) || !Equal(Number() += nn, n) || !Equal(Number() += n1, n))
			return OnError(18);
	}

	auto fn = [&](unsigned n, unsigned m) {
		n1.Set(m);
		uint64_t mm = m;
		n3 = n2 = num = n;
		const auto sum = n + m;
		return Equal(num + m, sum) && Equal(num + mm, sum) && Equal(num + n1, sum) &&
			Equal(num += m, sum) && Equal(n2 += mm, sum) && Equal(n3 += n1, sum);
	};
	auto fn64 = [&](uint64_t n, uint64_t m) {
		n1.Set(m);
		n2 = num = n;
		const auto sum = n + m;
		return (num + m).AsI64() == sum && (num + n1).AsI64() == sum &&
			(num += m).AsI64() == sum && (n2 += n1).AsI64() == sum;
	};

	for (unsigned n = 0; n <= 123; ++n)
	{
		for (unsigned m = 0; m <= 123; ++m)
		{
			if (!fn(n, m))
				return OnError(19);
		}
	}

	for (unsigned i = 0; i < 3000; ++i)
	{
		unsigned n = Rand() & 0x80000000, m = Rand() & 0x7fffffff;
		uint64_t nn = (static_cast<uint64_t>(Rand()) << 31) | m_Rg.UInt();
		uint64_t mm = (static_cast<uint64_t>(Rand()) << 31) | m_Rg.UInt();
		if (!fn(n, m) || !fn64(n, mm) || !fn64(nn, m) || !fn64(nn, mm))
			return OnError(20);

		if (!(i & 0x1f))
		{
			num = Rand();
			uint64_t sum = num.AsInt();
			for (unsigned j = 1; j <= 5; ++j)
			{
				num += num;
				if (num.AsI64() != (sum << j))
					return OnError(21);
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestSubtraction()
{
	// Тестируем операторы вычитания

	Number num, n1, n2, n3;
	auto fn = [&](unsigned n, unsigned m) {
		n1.Set(m);
		uint64_t mm = m;
		n3 = n2 = num = n;
		const auto dif = (n > m) ? n - m : 0;
		return Equal(num - m, dif) && Equal(num - mm, dif) && Equal(num - n1, dif) &&
			Equal(num -= m, dif) && Equal(n2 -= mm, dif) && Equal(n3 -= n1, dif);
	};
	auto fn64 = [&](uint64_t n, uint64_t m) {
		n1.Set(m);
		n2 = num = n;
		const auto dif = (n > m) ? n - m : 0;
		return (num - m).AsI64() == dif && (num - n1).AsI64() == dif &&
			(num -= m).AsI64() == dif && (n2 -= n1).AsI64() == dif;
	};

	for (unsigned n = 0; n <= 100; ++n)
	{
		for (unsigned m = 0; m <= n + 5; ++m)
		{
			if (!fn(n, m))
				return OnError(22);
		}
	}

	for (unsigned i = 0; i < 3000; ++i)
	{
		unsigned n = Rand(), m = Rand();
		uint64_t nn = (static_cast<uint64_t>(Rand()) << 32) | m_Rg.UInt();
		uint64_t mm = (static_cast<uint64_t>(Rand()) << 32) | m_Rg.UInt();
		if (!fn(n, m) || !fn64(n, mm) || !fn64(nn, m) || !fn64(nn, mm))
			return OnError(23);

		if (!(i & 0x1f))
		{
			num = Rand();
			for (unsigned j = 0; j < 3; ++j)
			{
				num -= num;
				if (num || !Equal(num, 0))
					return OnError(24);
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestGetHash()
{
	// Тестируем функцию GetHash

	Number num;
	for (unsigned n = 0; n <= 999; ++n)
	{
		num.Set(n);
		if (num.GetHash() != GetHash(num))
			return OnError(25);
	}

	auto fn = [&](char* p, size_t)
	{
		num.Set(p);
		return num.GetHash() == GetHash(num);
	};
	if (!ForRandomNumbers(4, 23, 100, fn))
		return OnError(26);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestIsPalindrome()
{
	// Тестируем функцию IsPalindrome

	auto CheckNum = [](const Number& num, const char* p) {
		const bool isPalindrome = num.IsPalindrome();
		for (size_t i = 0, j = strlen(p) - 1; i < j; ++i, --j)
		{
			if (p[i] != p[j])
				return !isPalindrome;
		}
		return isPalindrome;
	};

	Number num;
	char buffer[8];
	for (unsigned n = 0; n <= 12349; ++n)
	{
		num.Set(n);
		sprintf_s(buffer, "%u", n);
		if (!CheckNum(num, buffer))
			return OnError(27);
	}

	size_t counter = 0;
	auto fn = [&](char* p, size_t len) {
		if (++counter & 1)
		{
			for (size_t i = 0; i < len / 2; ++i)
				p[len - i - 1] = p[i];
		}
		num.Set(p);
		return CheckNum(num, p);
	};
	if (!ForRandomNumbers(5, 23, 100, fn))
		return OnError(28);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumber::TestGetKinNumberC()
{
	// Тестируем функцию GetKinNumberC

	Number num;
	auto fn = [&](char* p, size_t len) {
		num.Set(p);
		uint64_t counter = 0;
		for (size_t i = len / 2; i;)
		{
			if (p[i - 1] < '9' && p[len - i] > '0')
			{
				++p[i - 1];
				--p[len - i];
				for (; i < len / 2; ++i)
				{
					size_t f = p[i] - '0';
					size_t l = '9' - p[len - i - 1];
					char k = static_cast<char>((f < l) ? f : l);
					p[i] -= k, p[len - i - 1] += k;
				}
				++counter;
			} else
				--i;
		}
		return num.GetKinNumberC() == counter;
	};

	char buffer[40];
	for (unsigned n = 0; n <= 10099; ++n)
	{
		if (!fn(buffer, sprintf_s(buffer, "%u", n)))
			return OnError(29);
	}

	if (!ForRandomNumbers(5, 10, 100, fn))
		return OnError(30);

	uint64_t totalC = 90;
	// Проверим, что на больших числах нет переполнения
	// (макс. допустимая длина такого числа - 39 цифр)
	for (size_t len = 5; len < sizeof(buffer); ++len)
	{
		buffer[0] = '1';
		buffer[len] = 0;
		for (size_t i = 1; i < len; ++i)
			buffer[i] = (i < len / 2) ? '0' : '9';
		num.Set(buffer);
		totalC *= (len & 1) ? 1 : 10;
		if (1 + num.GetKinNumberC() != totalC)
			return OnError(31);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestFixNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool TestFixNumber::Execute()
{
	PrintHeader();

	if (!IsCancelled() && !TestCtorSet())
		return false;
	if (!IsCancelled() && !TestComparison())
		return false;
	if (!IsCancelled() && !TestGetHash())
		return false;

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFixNumber::TestCtorSet()
{
	// Тестируем конструкторы, операторы копирования, оператор bool(), функции *Zero и GetLength

	static const char* numberA[] = { "0", "1", "01", "12", "30", "123",
		"600", "090", "00005", "123456789012345678901234567890" };

	FixNumber f, f1, f2;
	Number num(123u), n1, n2, n3, n4, n5;
	auto fn = [&](const char* pNumber, size_t = 0) {
		num = f;
		if (f || !f.IsZero() || f.GetLength() != 1 || !Equal(num, 0))
			return false;
		n1 = f = num = pNumber;
		n2 = FixNumber(pNumber);
		n3 = FixNumber(num);
		n4 = f1 = pNumber;
		n5 = f2 = f;
		if (num.IsZero() != f.IsZero() || num.IsZero() != !f || num.GetLength() != f.GetLength() ||
			num != n1 || num != n2 || num != n3 || num != n4 || num != n5)
			return false;
		f.SetZero();
		return true;
	};

	for (auto number : numberA)
	{
		if (!fn(number))
			return OnError(1);
	}

	if (!ForRandomNumbers(1, 30, 100, fn))
		return OnError(2);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFixNumber::TestComparison()
{
	// Тестируем операторы сравнения

	auto baseFn = [](const FixNumber& n, const Number& m, int k) {
		return (n == m) == (k == 0) && (m == n) == (k == 0) &&
			(n != m) == (k != 0) && (m != n) == (k != 0);
	};
	auto fixFn = [](const FixNumber& n, const FixNumber& m, int k) {
		return (n == m) == (k == 0) && (n != m) == (k != 0) &&
			(n < m) == (k < 0) && (n <= m) == (k <= 0) &&
			(n > m) == (k > 0) && (n >= m) == (k >= 0);
	};

	for (unsigned i = 0; i <= 150; ++i)
	{
		Number num;
		const FixNumber n(i);
		for (unsigned j = 0; j <= 150; ++j)
		{
			FixNumber m = num = j;
			int diff = (i == j) ? 0 : (i < j) ? -1 : 1;
			if (!baseFn(n, num, diff) || !fixFn(n, m, diff))
				return OnError(3);
		}
	}

	Number n, m;
	for (int k = 0; k < 1000; ++k)
	{
		unsigned i = Rand();
		unsigned j = (k & 7) ? Rand() : i;
		FixNumber fn = n = i, fm = m = j;
		int diff = (i == j) ? 0 : (i < j) ? -1 : 1;
		if (!baseFn(fn, m, diff) || !fixFn(fn, fm, diff))
			return OnError(4);
	}

	unsigned counter = 0;
	auto fn = [&](char* p, size_t) {
		m = n;
		n.Set(p);
		if (!(counter++ & 7))
			m = n;
		FixNumber fn = n, fm = m;
		int diff = (n == m) ? 0 : (n < m) ? -1 : 1;
		if (!baseFn(fn, m, diff) || !fixFn(fn, fm, diff))
			return false;
		fm = m = Rand();
		diff = (n == m) ? 0 : (n < m) ? -1 : 1;
		return baseFn(fn, m, diff) && fixFn(fn, fm, diff) && fixFn(fm, fn, -diff);
	};
	if (!ForRandomNumbers(3, 30, 100, fn))
		return OnError(5);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFixNumber::TestGetHash()
{
	// Тестируем функцию GetHash

	Number num;
	for (unsigned n = 0; n <= 999; ++n)
	{
		FixNumber f = num = n;
		if (f.GetHash() != GetHash(num))
			return OnError(6);
	}

	auto fn = [&](char* p, size_t)
	{
		num.Set(p);
		FixNumber f(p);
		return f.GetHash() == GetHash(num);
	};
	if (!ForRandomNumbers(4, 23, 100, fn))
		return OnError(7);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestBigNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::Execute()
{
	PrintHeader();

	if (!IsCancelled() && !TestCtorCopyMove())
		return false;
	if (!IsCancelled() && !TestIncDec())
		return false;
	if (!IsCancelled() && !TestAddSub())
		return false;
	if (!IsCancelled() && !TestReversed())
		return false;
	if (!IsCancelled() && !TestReverseAndAdd())
		return false;
	if (!IsCancelled() && !TestRAATillPalindrome())
		return false;
	if (!IsCancelled() && !TestRAATillLength())
		return false;

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestCtorCopyMove()
{
	// Тестируем конструкторы, операторы копирования/перемещения

	for (int i = 0; i < 100; ++i)
	{
		char buffer[12];
		unsigned n = Rand();
		sprintf_s(buffer, "%u", n);
		std::string s(buffer);

		BigNumber n0, n1(buffer), n2(s);
		BigNumber n3(n1.AsInt()), n4(n2.AsI64());
		if (n0 || !Equal(n0, 0) || !Equal(n1, n) || !Equal(n2, n) || !Equal(n3, n) || !Equal(n4, n))
			return OnError(1);

		n1 = n = Rand();
		n2 = n1.AsI64();
		n3 = n2.AsString();
		n4 = n3.AsString().c_str();
		if (!Equal(n1, n) || !Equal(n2, n) || !Equal(n3, n) || !Equal(n4, n))
			return OnError(2);
	}

	BigNumber num;
	for (int i = 0; i < 100; ++i)
	{
		num.Set(Rand());
		Number base(num);
		BigNumber n1(num), n2(base);
		if (num != n1 || num != n2 || n1 != BigNumber(std::move(n2)))
			return OnError(3);
	}

	Number base;
	FixNumber fix;
	BigNumber n1, n2, n3;
	for (int i = 0; i < 100; ++i)
	{
		n1 = num = Rand();
		BigNumber numCopy(num);
		n2 = std::move(numCopy);
		if (num != n1 || num != n2)
			return OnError(4);

		n1 = base = Rand();
		n2 = fix = base;
		Number baseCopy(base);
		n3 = std::move(baseCopy);
		if (base != n1 || base != n2 || base != n3)
			return OnError(5);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestIncDec()
{
	// Тестируем операторы ++, --

	BigNumber n1, n2;
	for (int i = 0; i < 100; ++i)
	{
		unsigned n;
		n2 = n1 = n = 9 + (Rand() & 0x7fffffff);
		for (unsigned j = 1; j <= 9; ++j)
		{
			if (!Equal(++n1, n + j) || !Equal(--n2, n - j))
				return OnError(6);
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestAddSub()
{
	// Тестируем операторы сложения и вычитания

	BigNumber num, n1, n2, n3;
	auto addFn = [&](unsigned n, unsigned m) {
		n1.Set(m);
		uint64_t mm = m;
		n3 = n2 = num = n;
		const auto sum = n + m;
		return Equal(num + m, sum) && Equal(num + mm, sum) && Equal(num + n1, sum) &&
			Equal(num += m, sum) && Equal(n2 += mm, sum) && Equal(n3 += n1, sum);
	};
	auto subFn = [&](unsigned n, unsigned m) {
		n1.Set(m);
		uint64_t mm = m;
		n3 = n2 = num = n;
		const auto dif = (n > m) ? n - m : 0;
		return Equal(num - m, dif) && Equal(num - mm, dif) && Equal(num - n1, dif) &&
			Equal(num -= m, dif) && Equal(n2 -= mm, dif) && Equal(n3 -= n1, dif);
	};

	for (unsigned i = 0; i < 300; ++i)
	{
		unsigned n = Rand() & 0x80000000;
		unsigned m = Rand() & 0x7fffffff;
		if (!addFn(n, m) || !subFn(n, m))
			return OnError(7);
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestReversed()
{
	// Тестируем функции GetReversed и IsReversedLower

	BigNumber num;
	auto fn = [&](char* p, size_t len) {
		num.Set(p);
		const auto rev = num.GetReversed();
		const std::string s = rev.AsString();
		if (!rev.GetLength() || s.empty() || (rev < num) != num.IsReversedLower())
			return false;
		for (size_t i = 0, j = s.size(); i < len; ++i, j && --j)
		{
			if ((j && p[i] != s[j - 1]) || (!j && p[i] != '0'))
				return false;
		}
		return true;
	};

	char buffer[4];
	for (unsigned n = 0; n <= 999; ++n)
	{
		if (!fn(buffer, sprintf_s(buffer, "%u", n)))
			return OnError(8);
	}

	if (!ForRandomNumbers(4, 10, 100, fn))
		return OnError(9);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestReverseAndAdd()
{
	// Тестируем функцию ReverseAndAdd

	BigNumber num, n1;
	auto fn = [&](const char* p, size_t len)
	{
		num.Set(p);
		num.ReverseAndAdd();
		uint64_t sum = 0, k = 1;
		for (size_t i = 0; i < len; ++i, k *= 10)
			sum += k * (p[i] + p[len - i - 1] - 2 * '0');
		return num.AsI64() == sum;
	};

	char buffer[4];
	for (unsigned n = 0; n <= 999; ++n)
	{
		if (!fn(buffer, sprintf_s(buffer, "%u", n)))
			return OnError(10);
	}

	if (!ForRandomNumbers(4, 18, 200, fn))
		return OnError(11);

	auto raaFn = [&](const char* p, size_t)
	{
		num.Set(p);
		num += num.GetReversed();
		for (unsigned n = 2; n <= 15; ++n)
		{
			n1.Set(p);
			n1.ReverseAndAdd(n);
			num.ReverseAndAdd();
			if (num != n1)
				return false;
		}
		return true;
	};

	if (!ForRandomNumbers(1, 40, 100, raaFn))
		return OnError(12);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestRAATillPalindrome()
{
	// Тестируем функцию RAATillPalindrome

	BigNumber num, raa;
	auto baseFn = [&](const char* p, size_t) {
		raa = num = p;
		unsigned stepDoneC = 0;
		const unsigned stepToDoC = 45 + m_Rg.UInt(11);
		const bool isPalindrome = raa.RAATillPalindrome(stepToDoC, stepDoneC);
		for (unsigned stepC = 1; stepC <= stepToDoC; ++stepC)
		{
			num.ReverseAndAdd();
			if (num.IsPalindrome())
				return isPalindrome && stepC == stepDoneC && num == raa;
		}
		return !isPalindrome && stepDoneC == stepToDoC && num == raa;
	};
	if (!ForRandomNumbers(1, 40, 100, baseFn))
		return OnError(13);

	auto longFn = [&](unsigned n, size_t len, unsigned hash)
	{
		num.Set(n);
		unsigned stepDoneC = 0;
		return !num.RAATillPalindrome(10000, stepDoneC) && stepDoneC == 10000 &&
			num.GetLength() == len && num.GetHash() == hash;
	};
	num.Reserve(5000);
	// Проверим корректность вычислений для первых 3 базовых чисел Лишрел при 10 тыс. шагах
	if (!longFn(196, 4159, 0x5a9ae6eb) || !longFn(879, 4155, 0x4c07af64) || !longFn(1997, 4160, 0x982671f9))
		return OnError(14);

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumber::TestRAATillLength()
{
	// Тестируем функцию RAATillLength

	BigNumber num, raa;
	unsigned stepC = 5;
	if (!num.RAATillLength(5, stepC) || stepC != 1 || num)
		return OnError(15);

	num.Set(111u);
	if (num.RAATillLength(2, stepC) || stepC || num.RAATillLength(3, stepC) || stepC)
		return OnError(16);

	auto fn = [&](const char* p, size_t len) {
		raa = num = p;
		unsigned stepDoneC, stepC = 0;
		const size_t newLen = ((len > 3) ? len - 3 : 0) + m_Rg.UInt(29);
		const bool isPalindrome = raa.RAATillLength(newLen, stepDoneC);
		while (num.GetLength() < newLen)
		{
			++stepC;
			num.ReverseAndAdd();
			if (num.IsPalindrome())
				return isPalindrome && stepC == stepDoneC && num == raa;
		}
		return !isPalindrome && stepC == stepDoneC && num == raa;
	};
	if (!ForRandomNumbers(1, 40, 100, fn))
		return OnError(17);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestBigNumberSkipRAADups
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Тест TestBigNumberSkipRAADups проверяет корректность работы функции BigNumer::SkipRAADups. Основная задача
// теста - убедиться, что эта наша оптимизация перебора чисел корректна, и мы не пропустим никаких отложенных
// палиндромов. Смысл самой оптимизации заключается в том, что мы пропускаем числа, для которых результат первой
// операции RAA (первого шага) совпадает с результатом такой операции для числа, которое мы уже проверили раньше
// (т.е. любого числа, состоящего из того же количества цифр, но меньшего по значению)

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumberSkipRAADups::Execute()
{
	PrintHeader();

	if (!IsCancelled() && !TestMain())
		return false;
	if (!IsCancelled() && !TestHigh())
		return false;

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumberSkipRAADups::OnError(unsigned errorNum)
{
	if (errorNum && m_VerboseOutput)
	{
		Number num(errorNum);
		aux::Printf("  #12Test failed for %u-digit range at number %u\n", num.GetLength(), errorNum);
	}
	return TestBigNumber::OnError();
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumberSkipRAADups::TestMain()
{
	BigNumber num(1u), n;
	unsigned errorNum = 0;
	// В наборе all мы будем хранить значения, получаемые в результате однократного выполнения операции RAA
	// (Reverse-And-Add), для всех обрабатываемых чисел одного диапазона, т.е. среди чисел одинаковой длины
	NumSet all(MAIN_MAX_RANGE);
	// Переменная nextToProcess содержит число, которое должно быть обработано
	// следующим, или содержит 0, если такое число в данный момент неизвестно
	BigNumber nextToProcess(1u);
	// Счётчик processedC ведёт учёт обработанных чисел, т.е. чисел, для которых мы выполнили бы поиск отложенного
	// палиндрома; на этих числах "останавливается" SkipRAADups, и для этих чисел мы помещаем в набор all результат
	// применения к ним операции RAA. Счётчик skippedC ведёт учёт количества пропущенных чисел, т.е. чисел, через
	// которые мы "проскочили" благодаря SkipRAADups, и для которых мы бы не стали искать отложенный палиндром.
	// Счётчик kinC содержит сумму количеств родственных чисел для всех обработанных чисел
	size_t processedC = 0, skippedC = 0;
	uint64_t kinC = 0;

	// В этом цикле мы проверим все натуральные числа от 1- до 6-значных. Переменные: i - проверяемое число, j -
	// первое число следующего диапазона, k - количество чисел, которое должно быть обработано в этом диапазоне
	for (unsigned i = 1, j = 10, k = 9;; ++i, ++num)
	{
		errorNum = i;
		if (i == j)
		{
			const size_t doneRange = num.GetLength() - 1;
			// Диапазон проверен. Сравниваем количество обработанных чисел с "правильным" значением k (см. ниже).
			// Количество пропущенных чисел должно совпадать с количеством родственных, так как именно родственные
			// числа, большие текущего обрабатываемого, мы пропускаем с помошью SkipRAADups. Общее количество чисел
			// в каждом диапазоне (9, 90, 900, ...) на 1/10 меньше значения первого числа следующего диапазона
			if (processedC != k || skippedC != kinC || processedC + skippedC != j - j / 10)
			{
				if (m_VerboseOutput)
					aux::Printf("  #12Test failed for %u-digit range\n", doneRange);
				return OnError();
			}

			if (m_VerboseOutput)
			{
				aux::Printf("  Done for all %u-digit numbers, p=%u, s=%u\n",
					doneRange, processedC, skippedC);
			}
			// С каждым следующим диапазоном время проверки значительно увеличивается,
			// поэтому в рамках полного тестирования ограничимся 6-значным диапазоном
			if (doneRange == MAIN_MAX_RANGE)
				return true;

			// Начиная новый диапазон, нужно очистить набор all, так как значения RAA для некоторых чисел предыдущего
			// диапазона могут совпасть со значениями RAA для некоторых чисел в новом диапазоне. Например, числа 56 и
			// 110, результат RAA для обоих равен 121. Вообще это означает, что такие числа либо оба дадут один и тот
			// же палиндром (за одинаковое количество шагов), либо оба будут являться числами Лишрел одного потока.
			// Однако мы (в основной программе) собираем статистику отдельно для каждого из диапазонов, и, что ещё
			// важнее, SkipRAADups и GetKinNumberC работают корректно только внутри одного диапазона
			all.Clear(doneRange);
			// Следующее число, которое мы обработаем, - первое число нового диапазона
			nextToProcess.Set(j);
			j *= 10;

			// Начиная с 9 для 1-значных чисел (все числа от 1 до 9 дают уникальное значение RAA), в каждом следующем
			// диапазоне количество обрабатываемых чисел увеличивается либо в 10 раз (при переходе от чётного диапазона
			// к нечётному, например от 6-значного к 7-значному), либо в 1.9 раза (от нечётного к чётному, например от
			// 3-значных к 4-значным). В первом случае количество пар цифр не меняется при увеличении длины числа, а
			// вместо каждого существующего кандидата из множества обработанных чисел предыдущего диапазона получаются
			// по 10 кандидатов с новой центральной цифрой. Во втором случае центральная цифра (дававшая x10), в новом
			// диапазоне становится новой парой, количество комбинаций которой равно 19 (19/10=1.9). Это 10 комбинаций
			// 0X (X - любая цифра) + 9 комбинаций X9 (X > 0). При левой цифре, отличной от 0, правая цифра может быть
			// только 9, потому что иначе мы можем изменить цифры в паре так, что их сумма не изменится, но новая пара
			// даст то же самое значение RAA, а сама станет меньше и будет равна одной из предыдущих, например 10->01,
			// 21->03, 66->39, 98->89 и т.д. Единственное исключение из этого правила - это переход от 1- к 2-значным
			// числам, когда значение 9 меняется на 18, т.е. увеличивается ровно в 2, а не 1.9 раза. Объясняется это
			// тем, что центральная (и единственная) цифра 1-значного числа даёт только 9 комбинаций (все натуральные
			// однозначные числа), а не 10, как в случаях, когда помимо центральной цифры в числе есть пары. Перейдя
			// от единственной цифры к паре, полное количество комбинаций этой пары станет 18: из 19 возможных лишь
			// пара 00 недопустима, так как исходное 2-значное число не может состоять из двух нолей
			k = (doneRange & 1) ? 2 * k - k / 10 : 10 * k;
			kinC = processedC = skippedC = 0;
		}
		else if (!(i & 0xffff) && IsCancelled())
			return true;

		n = num;
		int counter = 0;
		n.ReverseAndAdd();
		if (all.Insert(n))
			++counter;

		n = num;
		n.SkipRAADups();
		if (n < num)
			break;
		else if (n > num)
		{
			if (!nextToProcess)
				nextToProcess = n;
			else if (n != nextToProcess)
				break;
			++skippedC;
		} else
		{
			--counter;
			++processedC;
			kinC += num.GetKinNumberC();
			if (nextToProcess && n != nextToProcess)
				break;
			nextToProcess.SetZero();
		}

		if (counter)
			break;
	}

	return OnError(errorNum);
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumberSkipRAADups::TestHigh()
{
	// Для остальных чисел до HIGH_MAX_RANGE-значных выполним упрощённую проверку

	unsigned long long startNum = 1;
	for (unsigned i = 0; i < MAIN_MAX_RANGE; ++i)
		startNum *= 10;

	// В целях экономии времени и памяти, ограничимся 8 знаками при
	// проверке уникальности значений RAA для "обрабатываемых" чисел
	constexpr unsigned MAX_SET_RANGE = 8;
	NumSet all(MAX_SET_RANGE);

	BigNumber num(startNum), n;
	uint64_t processedC = 0, skippedC = 0;
	for (unsigned i = 0, j = 2, k = 9;; ++i, ++num)
	{
		while (num.GetLength() >= j)
		{
			const unsigned doneRange = j - 1;
			if (doneRange > MAIN_MAX_RANGE)
			{
				uint64_t totalC = 9;
				for (unsigned r = 1; r < doneRange; ++r, totalC *= 10);
				if (processedC != k || processedC + skippedC != totalC)
				{
					if (m_VerboseOutput)
						aux::Printf("  #12Test failed for %u-digit range\n", doneRange);
					break;
				}

				if (m_VerboseOutput)
				{
					aux::Printf("  Done for all %u-digit numbers, p=%llu, s=%llu\n",
						doneRange, processedC, skippedC);
				}
				if (doneRange == HIGH_MAX_RANGE)
					return true;

				all.Clear(doneRange);
				processedC = skippedC = 0;
			}
			++j;
			k = (doneRange & 1) ? 2 * k - k / 10 : 10 * k;
		}
		if (!(i & 0xffff) && IsCancelled())
			return true;

		n = num;
		num.SkipRAADups();
		if (num < n)
			break;
		++processedC;
		skippedC += num.GetKinNumberC();

		if (j <= MAX_SET_RANGE + 1)
		{
			n = num;
			n.ReverseAndAdd();
			if (!all.Insert(n))
				break;
		}
	}

	return OnError();
}

//----------------------------------------------------------------------------------------------------------------------
TestBigNumberSkipRAADups::NumSet::NumSet(size_t maxDigitC)
{
	size_t size = 20000 >> 3;
	// Числа, которые мы будем хранить в этом наборе, - это результаты RAA для всех чисел одного диапазона,
	// старший из которых определяется значением maxDigitC (количество цифр числа). Так, для 4 цифр в числе
	// макс. значение RAA будет равно 19998 (20000, чтобы при делении на 8 получить полное количество байт)
	for (size_t i = 4; i < maxDigitC; ++i)
		size *= 10;

	m_NumA = new uint8_t[size];
	AML_FILLA(m_NumA, 0, size);
	m_Size = size;
}

//----------------------------------------------------------------------------------------------------------------------
TestBigNumberSkipRAADups::NumSet::~NumSet()
{
	AML_SAFE_DELETEA(m_NumA);
}

//----------------------------------------------------------------------------------------------------------------------
void TestBigNumberSkipRAADups::NumSet::Clear(size_t digitC)
{
	size_t size = 200 >> 3;
	for (size_t i = 2; i < digitC; ++i)
		size *= 10;
	AML_FILLA(m_NumA, 0, std::min(size, m_Size));
}

//----------------------------------------------------------------------------------------------------------------------
bool TestBigNumberSkipRAADups::NumSet::Insert(const Number& num)
{
	const unsigned n = num.AsInt();
	uint8_t* p = m_NumA + (n >> 3);
	bool exists = !(*p & (1 << (n & 7)));
	*p |= 1 << (n & 7);
	return exists;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SpeedTestP196RAA
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Тест SpeedTestP196RAA последовательно выполняет операцию Reverse-And-Add над числом 196, пока оно не достигнет
// длины в 1,000,000 знаков (2,415,836 итераций). Это позволяет оценить насколько быстро мы выполняем операцию RAA
// над большими числами при решении задач, свзяанных с "проблемой 196", а также поиском отложенных палиндромов.
// Результаты тестирования, 11 ноября 2020, Core i7-2600K 3.4GHz:
//   Цифр   100K   200K    300K    500K       1M
//   x86    1.7s   6.8s   15.2s   42.4s   2m 50s
//   x64    1.7s   6.7s   15.2s   42.2s   2m 49s

//----------------------------------------------------------------------------------------------------------------------
bool SpeedTestP196RAA::Execute()
{
	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	m_VerboseOutput = true;
	PrintHeader();

	constexpr size_t targetLen = 1000000;
	constexpr size_t nextLenStep = targetLen / 10;

	BigNumber num(196u);
	num.Reserve(targetLen);

	ThreadTime timer;
	uint32_t lastTick = ::GetTickCount();

	bool newLine = true;
	size_t it = 0, lastIt = 0;
	size_t nextLen = nextLenStep;
	while (num.GetLength() < targetLen)
	{
		unsigned stepDoneC;
		// С увеличением длины числа будет снижаться скорость. Чтобы сохранять частоту обновления
		// статистики на примерно одном уровне на протяжении всего теста, мы будем по мере роста
		// длины числа снижать количество выполняемых операций RAA за один вызов RAATillLength
		const size_t digitC = num.GetLength() + 160 + (targetLen - num.GetLength()) / 2048;
		num.RAATillLength((digitC < nextLen) ? digitC : nextLen, stepDoneC);
		it += stepDoneC;

		const size_t len = num.GetLength();
		const uint32_t tick = ::GetTickCount();
		if (tick - lastTick >= 500 || len >= nextLen)
		{
			const uint64_t ms100Elapsed = (timer.GetElapsed() + 50000) / 100000;
			const unsigned minutes = static_cast<unsigned>(ms100Elapsed / 600);
			const float seconds = .1f * (ms100Elapsed - 600 * minutes);

			if (len >= nextLen)
			{
				nextLen += nextLenStep;
				const float totalElapsed = .1f * ms100Elapsed;
				const float avSpeed = it / ((totalElapsed > .001f) ? totalElapsed : 1.f);
				aux::Printf("\r  Iteration #15#%9s#7, length #15#%9s#7, av. speed %s, elapsed: %um %.1fs\n",
					SeparateWithCommas(it).c_str(), SeparateWithCommas(len).c_str(), FormatSpeed(avSpeed).c_str(),
					minutes, seconds);
				newLine = true;
			}
			if (len < targetLen)
			{
				const float elapsed = .001f * (tick - lastTick);
				const float speed = (it - lastIt) / ((elapsed > .01f) ? elapsed : 1.f);
				aux::Printf("\r  Iteration #15#%9s#7, length #15#%9s#7, speed %s, elapsed: %um %.1fs \b",
					SeparateWithCommas(it).c_str(), SeparateWithCommas(len).c_str(), FormatSpeed(speed).c_str(),
					minutes, seconds);
				newLine = false;
				lastTick = tick;
				lastIt = it;
			}

			if (IsCancelled())
			{
				if (!newLine)
					aux::Print("\n");
				break;
			}
		}
	}

	PrintFooter();
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SpeedTestRAA
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Тест SpeedTestRAA выполняет проверку 9600 чисел из 13-значного диапазона аналогично тому, как это делается
// при поиске отложенных палиндромов в основном режиме работы программы, измеряя среднее время, затрачиваемое
// на проверку всех этих чисел, а также среднее время одной операции RAA. Тестирование, проведённое 18 ноября
// 2020 года (Intel Core i7-2600K 3.4GHz, TurboBoost включен), показало следующие результаты:
//   x86: best=50.657 ms, av=51.156 ms, av/step=25.605 ns
//   x64: best=41.841 ms, av=42.208 ms, av/step=21.127 ns

//----------------------------------------------------------------------------------------------------------------------
bool SpeedTestRAA::Execute()
{
	constexpr size_t NUMBER_C = 9600;
	constexpr unsigned RAND_SEED = 357;
	constexpr uint64_t FIRST_NUM = 1500718990000ull;

	::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	m_VerboseOutput = true;
	PrintHeader();

	m_Numbers.clear();
	m_Numbers.reserve(NUMBER_C);

	BigNumber num(FIRST_NUM);
	math::RandGen m_Rg(RAND_SEED);
	for (size_t i = 0; i < NUMBER_C; ++i)
	{
		++num;
		if (i && i % 300 == 0)
			num += (100000000ull + m_Rg.UInt()) << 5;
		num.SkipRAADups();
		m_Numbers.push_back(num);
	}

	uint64_t t1, t2, freq;
	double bestTime = 9.9e+50;
	uint64_t totalTime = 0, totalStepC = 0;
	uint32_t lastTick = 0;

	for (size_t loopC = 0; !IsCancelled();)
	{
		GetCounter(t1);
		totalStepC += RunTestLoop();
		GetCounterAndFrequency(t2, freq);

		++loopC;
		double elapsed = 1.0e+6 * (t2 - t1) / freq;
		totalTime += static_cast<uint64_t>(elapsed + .5);
		if (elapsed < bestTime)
			bestTime = elapsed;

		uint32_t tick = ::GetTickCount();
		if (tick - lastTick > 333)
		{
			lastTick = tick;
			double avLoopTime = .001 * totalTime / loopC;
			double avStepTime = 1000. * totalTime / totalStepC;
			aux::Printf("\r  # %06u: best=%.3f ms, av=%.3f ms, last=%.3f ms, av/step=%.3f ns",
				loopC, .001f * bestTime, avLoopTime, .001f * elapsed, avStepTime);
		}
	}
	aux::Print("\n");

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
std::string SpeedTestRAA::GetPrintedHeader() const
{
	return "Running RAA speedtest (long number sequence)";
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE size_t SpeedTestRAA::RunTestLoop()
{
	BigNumber num;
	size_t totalStepC = 0;
	for (const auto& n : m_Numbers)
	{
		num = n;
		unsigned stepC;
		if (!num.RAATillLength(CONSEQ_LEN, stepC))
		{
			totalStepC += stepC;
			num.RAATillPalindrome(SEARCH_DEPTH - stepC, stepC);
		}
		totalStepC += stepC;
	}
	return totalStepC;
}

//----------------------------------------------------------------------------------------------------------------------
void SpeedTestRAA::GetCounter(uint64_t& counter)
{
	LARGE_INTEGER t;
	::QueryPerformanceCounter(&t);
	counter = t.QuadPart;
}

//----------------------------------------------------------------------------------------------------------------------
void SpeedTestRAA::GetCounterAndFrequency(uint64_t& counter, uint64_t& frequency)
{
	LARGE_INTEGER t, f;
	::QueryPerformanceCounter(&t);
	::QueryPerformanceFrequency(&f);

	counter = t.QuadPart;
	frequency = std::max(f.QuadPart, 1ll);
}
