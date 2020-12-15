//⬪MDPN⬪
#include "pch.h"
#include "number.h"

#include <core/array.h>
#include <core/exception.h>

#include <emmintrin.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Number::Allocator
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class Number::Allocator final
{
public:
	static uint8_t* Alloc(size_t size)
	{
		#if AML_64BIT
			// 64-битная версия RTL VS2015 выделяет память, выровненную
			// по границе 16 байт, поэтому не требуется ничего делать
			return new uint8_t[size];
		#else
			uint8_t* p = new uint8_t[size + 16];
			uint8_t* pAligned = p + 16 - (reinterpret_cast<size_t>(p) & 15);
			*(reinterpret_cast<uint8_t**>(pAligned) - 1) = p;
			return pAligned;
		#endif
	}

	static void Free(uint8_t* p)
	{
		#if AML_64BIT
			delete[] p;
		#else
			p = *(reinterpret_cast<uint8_t**>(p) - 1);
			delete[] p;
		#endif
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Number
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Массив цифр для дефолтной инициализации числа. Гарантируется, что самый первый байт массива
// (младший разряд числа) всегда будет равен 0. Значение остальных байтов считаем неопределёным
uint8_t Number::s_ZeroDigits[sizeof(size_t)] = { 0 };

//----------------------------------------------------------------------------------------------------------------------
#define DEFINE_NUMBER_OP(CLASS, OP, FN, ARG) \
	CLASS CLASS::operator OP(ARG rhs) const { \
		CLASS num; \
		num.FN(this, rhs); \
		return num; \
	}

//----------------------------------------------------------------------------------------------------------------------
#define DEFINE_NUMBER_OPS(CLASS) \
	DEFINE_NUMBER_OP(CLASS, +, Add, unsigned) \
	DEFINE_NUMBER_OP(CLASS, +, Add, unsigned long long) \
	DEFINE_NUMBER_OP(CLASS, +, Add, const Number&) \
	DEFINE_NUMBER_OP(CLASS, -, Subtract, unsigned) \
	DEFINE_NUMBER_OP(CLASS, -, Subtract, unsigned long long) \
	DEFINE_NUMBER_OP(CLASS, -, Subtract, const Number&)

DEFINE_NUMBER_OPS(Number)

//----------------------------------------------------------------------------------------------------------------------
Number::Number(const Number& that)
	: m_Length(that.m_Length)
{
	Allocate(m_Length);
	CopyDigits(m_DigitA, that.m_DigitA);
}

//----------------------------------------------------------------------------------------------------------------------
Number::Number(Number&& that)
	: m_Length(that.m_Length)
	, m_MaxLength(that.m_MaxLength)
	, m_DigitA(that.m_DigitA)
{
	that.m_Length = 1;
	that.m_MaxLength = 0;
	that.m_DigitA = s_ZeroDigits;
}

//----------------------------------------------------------------------------------------------------------------------
Number::~Number()
{
	if (m_DigitA != s_ZeroDigits)
		Allocator::Free(m_DigitA);
}

//----------------------------------------------------------------------------------------------------------------------
void Number::SetZero()
{
	m_Length = 1;
	m_DigitA[0] = 0;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Number::Set(unsigned num)
{
	if (m_MaxLength < 10)
		Allocate(10);

	m_Length = Decode(m_DigitA, num);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Number::Set(unsigned long long num)
{
	if (m_MaxLength < 20)
		Allocate(20);

	m_Length = Decode(m_DigitA, num);
}

//----------------------------------------------------------------------------------------------------------------------
unsigned Number::GetHash() const
{
	constexpr uint32_t FNV_SEED = 0x811c9dc5;
	constexpr uint32_t FNV_PRIME = 0x01000193;

	uint32_t hash = FNV_SEED;
	const uint8_t* p = m_DigitA;
	for (size_t i = m_Length / 2; i; --i, p += 2)
	{
		hash = (hash ^ p[0]) * FNV_PRIME;
		hash = (hash ^ p[1]) * FNV_PRIME;
	}
	return (m_Length & 1) ? (hash ^ *p) * FNV_PRIME : hash;
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t Number::GetKinNumberC() const
{
	if (m_Length > 39)
		OnError("Too big number");

	uint64_t count = 0, k = 1;
	const size_t len = m_Length;
	const uint8_t* p = m_DigitA;
	for (size_t i = len / 2; i; --i)
	{
		size_t l = p[i - 1];
		size_t f = p[len - i];
		size_t m = (9 - f < l) ? 9 - f : l;
		count += k * m;
		k *= f - l + 2 * m + 1;
	}
	return count;
}

//----------------------------------------------------------------------------------------------------------------------
std::string Number::AsString() const
{
	util::SmartArray<char, 960> buffer(m_Length);
	const uint8_t* p = m_DigitA + m_Length;
	for (size_t i = 0; i < m_Length; ++i)
		buffer[i] = *(--p) + '0';

	return std::string(buffer, m_Length);
}

//----------------------------------------------------------------------------------------------------------------------
size_t Number::AsString(char* pBuffer, size_t bufferSize) const
{
	if (!pBuffer || bufferSize <= m_Length)
		OnError("Buffer is too small");

	const uint8_t* p = m_DigitA + m_Length;
	for (size_t i = 0; i < m_Length; ++i)
		pBuffer[i] = *(--p) + '0';

	pBuffer[m_Length] = 0;
	return m_Length;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned Number::AsInt() const
{
	if (IsGreater("4294967295", 10))
		OnError("Too big number");

	const uint8_t* p = m_DigitA;
	size_t sum = p[m_Length - 1];
	for (size_t i = m_Length - 1; i;)
		sum = sum * 10 + p[--i];
	return static_cast<unsigned>(sum);
}

//----------------------------------------------------------------------------------------------------------------------
uint64_t Number::AsI64() const
{
	if (IsGreater("18446744073709551615", 20))
		OnError("Too big number");

	const uint8_t* p = m_DigitA;
	uint64_t sum = p[m_Length - 1];
	for (size_t i = m_Length - 1; i;)
		sum = sum * 10 + p[--i];
	return sum;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator =(const Number& rhs)
{
	if (this != &rhs)
	{
		if (rhs.m_Length > m_MaxLength)
			Allocate(rhs.m_Length);
		m_Length = rhs.m_Length;
		CopyDigits(m_DigitA, rhs.m_DigitA);
	}
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator =(const FixNumber& rhs)
{
	uint32_t len = rhs.m_Length;
	if (len > m_MaxLength)
		Allocate(len);
	m_Length = len;
	rhs.Unpack(m_DigitA);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator =(Number&& rhs)
{
	if (this != &rhs)
	{
		m_Length = rhs.m_Length;
		rhs.m_Length = 1;

		uint32_t myMaxLength = m_MaxLength;
		m_MaxLength = rhs.m_MaxLength;
		rhs.m_MaxLength = myMaxLength;

		uint8_t* myDigitA = m_DigitA;
		m_DigitA = rhs.m_DigitA;
		rhs.m_DigitA = myDigitA;
	}
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator +=(unsigned rhs)
{
	uint8_t buf[10];
	const uint32_t len = Decode(buf, rhs);
	if (len > m_MaxLength)
		Allocate(10, true);
	Add(m_DigitA, buf, len);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator +=(unsigned long long rhs)
{
	uint8_t buf[20];
	const uint32_t len = Decode(buf, rhs);
	if (len > m_MaxLength)
		Allocate(20, true);
	Add(m_DigitA, buf, len);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator +=(const Number& rhs)
{
	if (rhs.m_Length > m_MaxLength)
		Allocate(rhs.m_Length, true);
	Add(m_DigitA, rhs.m_DigitA, rhs.m_Length);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator -=(unsigned rhs)
{
	uint8_t buf[10];
	Subtract(m_DigitA, buf, Decode(buf, rhs));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator -=(unsigned long long rhs)
{
	uint8_t buf[20];
	Subtract(m_DigitA, buf, Decode(buf, rhs));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator ++()
{
	if (!m_MaxLength)
	{
		Allocate(1);
		m_DigitA[0] = 1;
		return *this;
	}

	uint8_t* p = m_DigitA;
	for (size_t n, i = m_Length; i; --i)
	{
		n = *p + 1;
		*p = n & 0xff;
		if (n <= 9)
			return *this;
		*p++ = 0;
	}
	if (m_Length >= m_MaxLength)
		Allocate(m_Length + 1, true);
	m_DigitA[m_Length++] = 1;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
Number& Number::operator --()
{
	if (m_Length > 1)
	{
		uint8_t* p = m_DigitA;
		for (size_t n, i = m_Length; i; --i)
		{
			n = *p - 1;
			*p = n & 0xff;
			if (n < 9)
				break;
			*p++ = 9;
		}
		if (!m_DigitA[m_Length - 1])
			--m_Length;
	}
	else if (m_DigitA[0])
		--m_DigitA[0];

	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator ==(const Number& rhs) const
{
	return m_Length == rhs.m_Length && !memcmp(m_DigitA, rhs.m_DigitA, m_Length);
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator !=(const Number& rhs) const
{
	return m_Length != rhs.m_Length || memcmp(m_DigitA, rhs.m_DigitA, m_Length);
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator <(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length < rhs.m_Length;

	for (size_t i = m_Length; i; --i)
	{
		if (int dif = m_DigitA[i - 1] - rhs.m_DigitA[i - 1])
			return dif < 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator <=(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length < rhs.m_Length;

	for (size_t i = m_Length; i; --i)
	{
		if (int dif = m_DigitA[i - 1] - rhs.m_DigitA[i - 1])
			return dif < 0;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator >(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length > rhs.m_Length;

	for (size_t i = m_Length; i; --i)
	{
		if (int dif = m_DigitA[i - 1] - rhs.m_DigitA[i - 1])
			return dif > 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::operator >=(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length > rhs.m_Length;

	for (size_t i = m_Length; i; --i)
	{
		if (int dif = m_DigitA[i - 1] - rhs.m_DigitA[i - 1])
			return dif > 0;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void Number::Allocate(uint32_t newLength, bool copy)
{
	const uint32_t mask = sizeof(size_t) - 1;
	newLength = (newLength + mask) & ~mask;

	uint8_t* pOld = m_DigitA;
	m_DigitA = Allocator::Alloc(newLength);
	m_MaxLength = newLength;

	if (copy)
		CopyDigits(m_DigitA, pOld);

	if (pOld != s_ZeroDigits)
		Allocator::Free(pOld);
}

//----------------------------------------------------------------------------------------------------------------------
void Number::CopyDigits(void* pTo, const void* pFrom)
{
	if (m_Length < 32)
	{
		size_t* pOut = reinterpret_cast<size_t*>(pTo);
		const size_t* pIn = reinterpret_cast<const size_t*>(pFrom);
		const size_t n = (m_Length + sizeof(size_t) - 1) / sizeof(size_t);
		for (size_t i = 0; i < n; ++i)
			pOut[i] = pIn[i];
	} else
		memcpy(pTo, pFrom, (m_Length + 3) & ~3);
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Number::Set(const char* pStr, size_t size)
{
	if (!size)
		OnError("Empty string");
	if (size > 0xffffffe8)
		OnError("Too long string");

	// Пропустим ноли в старших разрядах
	while (*pStr == '0' && size > 1)
		++pStr, --size;

	if (size > m_MaxLength)
		Allocate(static_cast<uint32_t>(size));
	m_Length = static_cast<uint32_t>(size);

	const char* p = pStr + size;
	for (size_t i = 0; i < size; ++i)
	{
		const char c = *(--p);
		if (c < '0' || c > '9')
		{
			m_Length = 1;
			m_DigitA[0] = 0;
			OnError("Not a valid number");
		}
		m_DigitA[i] = c - '0';
	}
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t Number::Decode(uint8_t* digitA, unsigned num)
{
	size_t n, len = 0;
	do {
		n = num;
		num /= 10;
		digitA[len++] = static_cast<uint8_t>(n - 10 * num);
	} while (num);

	return static_cast<uint32_t>(len);
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t Number::Decode(uint8_t* digitA, unsigned long long num)
{
	unsigned long long n;
	size_t len = 0;
	do {
		n = num;
		num /= 10;
		digitA[len++] = static_cast<uint8_t>(n - 10 * num);
	} while (num);

	return static_cast<uint32_t>(len);
}

//----------------------------------------------------------------------------------------------------------------------
void Number::Add(const Number* pLhs, const Number& rhs)
{
	Allocate(std::max(pLhs->m_Length, rhs.m_Length));
	m_Length = pLhs->m_Length;
	Add(pLhs->m_DigitA, rhs.m_DigitA, rhs.m_Length);
}

//----------------------------------------------------------------------------------------------------------------------
template<class T> void Number::Add(const Number* pLhs, const T& rhs)
{
	uint8_t buf[20];
	const uint32_t len = Decode(buf, rhs);
	Allocate(std::max(pLhs->m_Length, len));
	m_Length = pLhs->m_Length;
	Add(pLhs->m_DigitA, buf, len);
}

//----------------------------------------------------------------------------------------------------------------------
void Number::Add(const uint8_t* lhsA, const uint8_t* rhsA, size_t rhsLen)
{
	const size_t lhsLen = m_Length;
	const size_t len = std::max(lhsLen, rhsLen);
	m_Length = static_cast<uint32_t>(len);

	size_t carry = 0;
	for (size_t n, i = 0; i < len; ++i)
	{
		n = carry;
		if (i < lhsLen)
			n += lhsA[i];
		if (i < rhsLen)
			n += rhsA[i];
		carry = (n < 10) ? 0 : 1;
		m_DigitA[i] = static_cast<uint8_t>(n - 10 * carry);
	}

	if (carry)
	{
		if (m_Length >= m_MaxLength)
			Allocate(m_Length + 1, true);
		m_DigitA[m_Length++] = 1;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void Number::Subtract(const Number* pLhs, const Number& rhs)
{
	Allocate(pLhs->m_Length);
	m_Length = pLhs->m_Length;
	Subtract(pLhs->m_DigitA, rhs.m_DigitA, rhs.m_Length);
}

//----------------------------------------------------------------------------------------------------------------------
template<class T> void Number::Subtract(const Number* pLhs, const T& rhs)
{
	uint8_t buf[20];
	Allocate(pLhs->m_Length);
	m_Length = pLhs->m_Length;
	Subtract(pLhs->m_DigitA, buf, Decode(buf, rhs));
}

//----------------------------------------------------------------------------------------------------------------------
void Number::Subtract(const uint8_t* lhsA, const uint8_t* rhsA, size_t rhsLen)
{
	if (m_Length >= rhsLen)
	{
		size_t carry = 0, len = m_Length;
		for (size_t n, i = 0; i < len; ++i)
		{
			n = lhsA[i];
			n -= carry;
			if (i < rhsLen)
				n -= rhsA[i];
			carry = (n < 10) ? 0 : 1;
			m_DigitA[i] = static_cast<uint8_t>(n + 10 * carry);
		}

		if (!carry)
		{
			while (!m_DigitA[len - 1] && len > 1)
				--len;
			m_Length = static_cast<uint32_t>(len);
			return;
		}
	}
	m_Length = 1;
	m_DigitA[0] = 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::IsGreater(const char* digitA, size_t len) const
{
	if (m_Length != len)
		return m_Length > len;

	for (size_t i = len; i; --i)
	{
		if (int dif = m_DigitA[i - 1] - digitA[len - i] + '0')
			return dif > 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool Number::IsPalindrome(const uint8_t* digitA, size_t len)
{
	const size_t* pF = reinterpret_cast<const size_t*>(digitA);
	const size_t* pL = reinterpret_cast<const size_t*>(digitA + len) - 1;

	while (pF <= pL)
	{
		#if AML_64BIT
			if (util::ByteSwap64(*pF++) != *pL--)
				return false;
		#else
			if (util::ByteSwap32(*pF++) != *pL--)
				return false;
		#endif
	}

	const uint8_t* pFb = reinterpret_cast<const uint8_t*>(pF);
	const uint8_t* pLb = reinterpret_cast<const uint8_t*>(pL + 1) - 1;

	#if AML_64BIT
		while (pFb < pLb)
		{
			if (*pFb++ != *pLb--)
				return false;
		}
		return true;
	#else
		return pFb >= pLb || *pFb == *pLb;
	#endif
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Number::OnError(const char* pMsg)
{
	throw util::ELogic(pMsg ? pMsg : "Unknown error");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FixNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_LITTLE_ENDIAN
	#define FXNUM_PACKW(V) static_cast<uint8_t>((V) | ((V) >> 4))
#else
	#define FXNUM_PACKW(V) static_cast<uint8_t>(((V) << 4) | ((V) >> 8))
#endif

//----------------------------------------------------------------------------------------------------------------------
unsigned FixNumber::GetHash() const
{
	constexpr uint32_t FNV_SEED = 0x811c9dc5;
	constexpr uint32_t FNV_PRIME = 0x01000193;

	uint32_t hash = FNV_SEED;
	const size_t len = m_Length;
	for (size_t i = 0; i < len / 2; ++i)
	{
		const uint32_t v = m_DigitA[1 + i];
		hash = (hash ^ (v & 0xf)) * FNV_PRIME;
		hash = (hash ^ (v >> 4)) * FNV_PRIME;
	}
	return (len & 1) ? (hash ^ m_DigitA[1 + len / 2]) * FNV_PRIME : hash;
}

//----------------------------------------------------------------------------------------------------------------------
FixNumber& FixNumber::operator =(const Number& rhs)
{
	if (rhs.m_Length > MAX_LENGTH)
		Number::OnError("Too big number");

	const size_t len = rhs.m_Length;
	m_Length = static_cast<uint8_t>(len);

	const uint8_t* pRhs = rhs.m_DigitA;
	for (size_t i = 0; i < len / 2; ++i, pRhs += 2)
	{
		size_t v = *reinterpret_cast<const uint16_t*>(pRhs);
		m_DigitA[1 + i] = FXNUM_PACKW(v);
	}
	if (len & 1)
		m_DigitA[1 + len / 2] = *pRhs;

	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
FixNumber& FixNumber::operator =(const char* pNum)
{
	if (!pNum || !pNum[0])
		Number::OnError("Empty string");

	// Пропустим ноли в старших разрядах
	while (pNum[0] == '0' && pNum[1])
		++pNum;

	size_t len;
	for (len = 0; pNum[len]; ++len)
	{
		if (pNum[len] < '0' || pNum[len] > '9')
			Number::OnError("Not a valid number");
	}
	if (len > MAX_LENGTH)
		Number::OnError("Too long string");
	m_Length = static_cast<uint8_t>(len);

	const char* p = pNum + len - 1;
	for (size_t i = 0; i < len / 2; ++i, p -= 2)
	{
		size_t v = *reinterpret_cast<const uint16_t*>(p - 1) - 0x3030;
		#if AML_LITTLE_ENDIAN
			m_DigitA[1 + i] = static_cast<uint8_t>((v << 4) | (v >> 8));
		#else
			m_DigitA[1 + i] = static_cast<uint8_t>(v | (v >> 4));
		#endif
	}
	if (len & 1)
		m_DigitA[1 + len / 2] = *p - '0';

	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator ==(const FixNumber& rhs) const
{
	const uint8_t* pL = m_DigitA;
	const uint8_t* pR = rhs.m_DigitA;
	const size_t len = (m_Length + 3) / 2;
	for (size_t i = 0; i < len / 4; ++i)
	{
		if (*reinterpret_cast<const uint32_t*>(pL) != *reinterpret_cast<const uint32_t*>(pR))
			return false;
		pL += 4;
		pR += 4;
	}
	for (size_t i = 0; i < (len & 3); ++i)
	{
		if (*pL++ != *pR++)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator !=(const FixNumber& rhs) const
{
	const uint8_t* pL = m_DigitA;
	const uint8_t* pR = rhs.m_DigitA;
	const size_t len = (m_Length + 3) / 2;
	for (size_t i = 0; i < len / 4; ++i)
	{
		if (*reinterpret_cast<const uint32_t*>(pL) != *reinterpret_cast<const uint32_t*>(pR))
			return true;
		pL += 4;
		pR += 4;
	}
	for (size_t i = 0; i < (len & 3); ++i)
	{
		if (*pL++ != *pR++)
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator ==(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return false;

	const size_t len = m_Length;
	const uint8_t* pRhs = rhs.m_DigitA;
	for (size_t i = 0; i < len / 2; ++i, pRhs += 2)
	{
		size_t v = *reinterpret_cast<const uint16_t*>(pRhs);
		if (m_DigitA[1 + i] != FXNUM_PACKW(v))
			return false;
	}
	return !(len & 1) || m_DigitA[1 + len / 2] == *pRhs;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator !=(const Number& rhs) const
{
	if (m_Length != rhs.m_Length)
		return true;

	const size_t len = m_Length;
	const uint8_t* pRhs = rhs.m_DigitA;
	for (size_t i = 0; i < len / 2; ++i, pRhs += 2)
	{
		size_t v = *reinterpret_cast<const uint16_t*>(pRhs);
		if (m_DigitA[1 + i] != FXNUM_PACKW(v))
			return true;
	}
	return (len & 1) && m_DigitA[1 + len / 2] != *pRhs;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator <(const FixNumber& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length < rhs.m_Length;

	for (size_t i = (m_Length + 1) / 2; i; --i)
	{
		if (int dif = m_DigitA[i] - rhs.m_DigitA[i])
			return dif < 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator <=(const FixNumber& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length < rhs.m_Length;

	for (size_t i = (m_Length + 1) / 2; i; --i)
	{
		if (int dif = m_DigitA[i] - rhs.m_DigitA[i])
			return dif < 0;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator >(const FixNumber& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length > rhs.m_Length;

	for (size_t i = (m_Length + 1) / 2; i; --i)
	{
		if (int dif = m_DigitA[i] - rhs.m_DigitA[i])
			return dif > 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FixNumber::operator >=(const FixNumber& rhs) const
{
	if (m_Length != rhs.m_Length)
		return m_Length > rhs.m_Length;

	for (size_t i = (m_Length + 1) / 2; i; --i)
	{
		if (int dif = m_DigitA[i] - rhs.m_DigitA[i])
			return dif > 0;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
size_t FixNumber::Unpack(uint8_t* digitA) const
{
	uint8_t* p = digitA;
	const size_t len = m_Length;
	for (size_t i = 0; i < len / 2; ++i, p += 2)
	{
		const size_t v = m_DigitA[1 + i];
		p[0] = static_cast<uint8_t>(v & 0xf);
		p[1] = static_cast<uint8_t>(v >> 4);
	}
	if (len & 1)
		*p = m_DigitA[1 + len / 2];
	return len;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BigNumber
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
#define RAA_SUM32(POUT) \
	carry += 0xf6f6f6f6; \
	front = util::ByteSwap32(front); \
	uint32_t sum = static_cast<uint32_t>(front + back + carry); \
	carry = ~sum >> 31; \
	uint32_t mask = sum & 0x60606060; \
	*(POUT) = (sum & 0x0f0f0f0f) - (mask >> 4);

//----------------------------------------------------------------------------------------------------------------------
#define RAA_SUM64(POUT) \
	carry += 0xf6f6f6f6f6f6f6f6ull; \
	front = util::ByteSwap64(front); \
	uint64_t sum = front + back + carry; \
	carry = ~sum >> 63; \
	uint64_t mask = sum & 0x6060606060606060ull; \
	*(POUT) = (sum & 0x0f0f0f0f0f0f0f0full) - (mask >> 4);

DEFINE_NUMBER_OPS(BigNumber)

//----------------------------------------------------------------------------------------------------------------------
BigNumber::BigNumber(const BigNumber& that)
	: Number(that)
{
}

//----------------------------------------------------------------------------------------------------------------------
BigNumber::BigNumber(const Number& that)
	: Number(that)
{
}

//----------------------------------------------------------------------------------------------------------------------
BigNumber::BigNumber(BigNumber&& that)
	: Number(std::move(that))
	, m_pRAABuffer(that.m_pRAABuffer)
	, m_RAABufferSize(that.m_RAABufferSize)
{
	that.m_pRAABuffer = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
BigNumber::~BigNumber()
{
	if (m_pRAABuffer)
		Allocator::Free(m_pRAABuffer);
}

//----------------------------------------------------------------------------------------------------------------------
void BigNumber::Reserve(size_t maxLength)
{
	if (maxLength > 0xffffffb0)
		OnError("Length is too big");

	if (maxLength > m_MaxLength)
		Allocate(static_cast<uint32_t>(maxLength), true);

	if (maxLength > LOCAL_RAA_BUFFER_SIZE)
		AllocateRAABuffer(maxLength);
}

//----------------------------------------------------------------------------------------------------------------------
void BigNumber::SkipRAADups()
{
	if (m_Length > 1)
	{
		uint8_t* p = m_DigitA;
		const size_t len = m_Length;
		for (size_t i = len / 2 - 1; i; --i)
		{
			if (p[len - 1 - i] && p[i] < 9)
			{
				p[0] = (p[len - 1] > 1) ? 9 : 0;
				for (size_t j = 1; j < i; ++j)
					p[j] = p[len - 1 - j] ? 9 : 0;
				p[i] = 9;
				return;
			}
		}
		if (p[len - 1] > 1)
			p[0] = 9;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool BigNumber::RAATillPalindrome(unsigned stepC, unsigned& doneC)
{
	unsigned count = m_MaxLength ? RAA(stepC, true) : 1;
	doneC = count ? count : stepC;
	return count != 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool BigNumber::RAATillLength(size_t length, unsigned& doneC)
{
	doneC = 0;
	if (m_Length < length)
	{
		if (length > 0x80000000)
			OnError("Length is too big");

		if (!m_MaxLength)
		{
			doneC = 1;
			return true;
		}

		while (m_Length < length)
		{
			unsigned stepC = static_cast<unsigned>(length) - m_Length;
			// В среднем, для увеличения длины числа на 1 знак требуется около 2 операций RAA. Но в
			// зависимости от требуемой разницы длин, это значение неодинаково. Так, 1 операция может
			// увеличить длину числа на 1, 2 операции - на 2, 3 операции - на 3, но для увеличения длины
			// числа на 4 знака необходимо не менее 5 (чаще больше) операций RAA, и т.д. Используя это
			// простое приближение мы сможем значительно сократить количество вызовов функции RAA
			stepC = (stepC > 4) ? 2 * stepC - 2 : stepC + stepC / 4;

			if (unsigned count = RAA(stepC, true))
			{
				doneC += count;
				return true;
			}
			doneC += stepC;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool BigNumber::IsReversedLower() const
{
	const uint8_t* pF = m_DigitA;
	const uint8_t* pL = pF + m_Length - 1;

	while (pF < pL)
	{
		if (int dif = *pF++ - *pL--)
			return dif < 0;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
BigNumber BigNumber::GetReversed() const
{
	uint32_t len = m_Length;
	const uint8_t* pIn = m_DigitA;
	// Ноли из младших разрядов исходного числа не должны
	// появиться в старших разрядах обратного числа
	while (*pIn == 0 && len > 1)
		++pIn, --len;

	BigNumber num;
	num.Allocate(len);
	num.m_Length = len;

	uint8_t* pOut = num.m_DigitA;
	for (size_t i = len; i; --i)
		*pOut++ = pIn[i - 1];

	return num;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint8_t* BigNumber::AllocateRAABuffer(size_t maxLength)
{
	if (!m_pRAABuffer || maxLength > m_RAABufferSize)
	{
		if (m_pRAABuffer)
		{
			Allocator::Free(m_pRAABuffer);
			m_pRAABuffer = nullptr;
		}
		maxLength = (maxLength + 79) & ~15;
		m_pRAABuffer = Allocator::Alloc(maxLength);
		m_RAABufferSize = maxLength;
	}
	return m_pRAABuffer;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned BigNumber::RAA(unsigned stepC, bool stopOnPalindrome)
{
	uint8_t* pDig1 = m_DigitA;
	const size_t maxLength = m_Length + stepC / 2 + 2;
	alignas(16) uint8_t localBuffer[(LOCAL_RAA_BUFFER_SIZE + 7) & ~size_t(7)];
	uint8_t* pDig2 = (maxLength <= LOCAL_RAA_BUFFER_SIZE) ? localBuffer : AllocateRAABuffer(maxLength);

	const __m128i maskBs = _mm_set_epi64x(0x0001020304050607ull, 0x08090a0b0c0d0e0full);
	const __m128i maskF6 = _mm_set1_epi64x(0xf6f6f6f6f6f6f6f6ull);
	const __m128i maskF7 = _mm_set1_epi64x(0xf6f6f6f6f6f6f6f7ull);
	const __m128i maskCr = _mm_set1_epi64x(0x80000000);

	unsigned doneC = 0;
	for (unsigned step = 1; step <= stepC; ++step)
	{
		const uint32_t* pF = reinterpret_cast<uint32_t*>(pDig1);
		const uint32_t* pL = reinterpret_cast<uint32_t*>(pDig1 + m_Length);
		uint32_t* pOut = reinterpret_cast<uint32_t*>(pDig2);

		size_t carry = 0;
		// Алгоритм RAA использует инструкции SSSE3
		if (size_t i = m_Length / 16)
		{
			__m128i cr = _mm_unpacklo_epi64(maskF6, maskF7);

			do {
				pL -= 4;
				// 16 байт левой части массива (младшие 16 разрядов числа) складываем с
				// "перевёрнутыми" 16 байтами правой части массива (старшие 16 разрядов числа)
				// и добавляем к сумме значение cr, которое содержит маску 0xf6..f6 и флаг переноса
				__m128i sum = _mm_add_epi64(_mm_add_epi64(_mm_load_si128(reinterpret_cast<const __m128i*>(pF)), cr),
					_mm_shuffle_epi8(_mm_lddqu_si128(reinterpret_cast<const __m128i*>(pL)), maskBs));
				// При возникновении переноса в младших 64 битах, добавляем 1 к старшим 64 битам
				sum = _mm_add_epi64(sum, _mm_shuffle_epi32(_mm_cmplt_epi32(sum, maskCr), 0x50));
				// Вычисляем маску переносов в разрядах (0x00 - был, 0xff - не было)
				const __m128i mask = _mm_cmplt_epi8(sum, _mm_setzero_si128());
				// Вычитаем 0xf6 из разрядов, в которых не было переноса, и сохраняем результат в выходной массив
				_mm_store_si128(reinterpret_cast<__m128i*>(pOut), _mm_sub_epi8(sum, _mm_and_si128(mask, maskF6)));
				// Формируем новое значение cr, учитывая перенос в старших 64 битах
				cr = _mm_add_epi8(_mm_srli_si128(mask, 15), maskF7);
				pF += 4; pOut += 4;
			} while (--i);

			carry = _mm_extract_epi16(cr, 0) & 1;
		}

		#if AML_64BIT
			if (m_Length & 8)
			{
				pL -= 2;
				uint64_t front = *reinterpret_cast<const uint64_t*>(pL);
				uint64_t back = *reinterpret_cast<const uint64_t*>(pF);
				RAA_SUM64(reinterpret_cast<uint64_t*>(pOut));
				pF +=2; pOut += 2;
			}
			if (m_Length & 4)
			{
				uint32_t front = *(--pL);
				uint32_t back = *pF++;
				RAA_SUM32(pOut++);
			}
		#else
			for (size_t i = (m_Length / 4) & 3; i; --i)
			{
				uint32_t front = *(--pL);
				uint32_t back = *pF++;
				RAA_SUM32(pOut++);
			}
		#endif

		const uint8_t* pFb = reinterpret_cast<const uint8_t*>(pF);
		const uint8_t* pLb = reinterpret_cast<const uint8_t*>(pL);
		uint8_t* pOb = reinterpret_cast<uint8_t*>(pOut);

		for (size_t i = m_Length & 3; i; --i)
		{
			const size_t v = carry + *(--pLb) + *pFb++;
			const size_t vc = v - 10;
			*pOb++ = static_cast<uint8_t>((v < 10) ? v : vc);
			carry = (v < 10) ? 0 : 1;
		}

		if (carry)
		{
			if (m_Length >= m_MaxLength)
			{
				const bool f = step & 1;
				Allocate(m_Length + 64, !f);
				(f ? pDig1 : pDig2) = m_DigitA;
			}
			pDig2[m_Length++] = 1;
		}

		uint8_t* t = pDig1;
		pDig1 = pDig2;
		pDig2 = t;

		if (stopOnPalindrome && IsPalindrome(pDig1, m_Length))
		{
			doneC = step;
			break;
		}
	}

	if (pDig1 != m_DigitA)
		memcpy(m_DigitA, pDig1, m_Length);

	return doneC;
}
