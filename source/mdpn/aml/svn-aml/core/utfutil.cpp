#include "utfutil.h"

#include "../../core/memwriter.h"
#include "../../core/platform.h"

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции конвертации UTF-8 в UTF-16 и UTF-32
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_USEASM

extern "C" {
	size_t AML_FASTCALL U8To16Len(const char* pStr, size_t size);
	size_t AML_FASTCALL U8To32Len(const char* pStr, size_t size);
}

#else

//----------------------------------------------------------------------------------------------------------------------
#define UTF8LEN_CHECKWORD(TYPE, MASK, SIZE) \
	if (*reinterpret_cast<const TYPE*>(pIn) & MASK) break;	\
	pIn += SIZE; inLeft -= SIZE; charsWritten += SIZE;

//----------------------------------------------------------------------------------------------------------------------
static size_t U8To16Len(const char* pStr, size_t size)
{
	size_t charsWritten = 0;
	const char* pIn = pStr;
	size_t inLeft = size;

	if ((reinterpret_cast<uintptr_t>(pIn) & 3) == 0)
	{
		while (inLeft >= 8)
		{
			UTF8LEN_CHECKWORD(uint32_t, 0x80808080, 4);
			UTF8LEN_CHECKWORD(uint32_t, 0x80808080, 4);
		}
	}

	if (inLeft)
	{
		for (uint32_t cp;;)
		{
			cp = *pIn++;
			++charsWritten;
			if (cp <= 0x7f)
			{
				--inLeft;
				if ((reinterpret_cast<uintptr_t>(pIn) & 1) == 0)
				{
					while (inLeft >= 4)
					{
						UTF8LEN_CHECKWORD(uint16_t, 0x8080, 2);
						UTF8LEN_CHECKWORD(uint16_t, 0x8080, 2);
					}
				}
				if (inLeft == 0) break;
			}
			else if ((cp & 0xe0) == 0xc0)
			{
				if (inLeft <= 2) break;
				++pIn; inLeft -= 2;
			}
			else if ((cp & 0xf0) == 0xe0)
			{
				if (inLeft <= 3) break;
				pIn += 2; inLeft -= 3;
			}
			else if ((cp & 0xf8) == 0xf0)
			{
				++charsWritten;
				if (inLeft <= 4) break;
				pIn += 3; inLeft -= 4;
			} else
				return 0;
		}
	}

	return charsWritten;
}

//----------------------------------------------------------------------------------------------------------------------
static size_t U8To32Len(const char* pStr, size_t size)
{
	size_t charsWritten = 0;
	const char* pIn = pStr;
	size_t inLeft = size;

	if ((reinterpret_cast<uintptr_t>(pIn) & 3) == 0)
	{
		while (inLeft >= 8)
		{
			UTF8LEN_CHECKWORD(uint32_t, 0x80808080, 4);
			UTF8LEN_CHECKWORD(uint32_t, 0x80808080, 4);
		}
	}

	if (inLeft)
	{
		for (uint32_t cp;;)
		{
			cp = *pIn++;
			++charsWritten;
			if (cp <= 0x7f)
			{
				--inLeft;
				if ((reinterpret_cast<uintptr_t>(pIn) & 1) == 0)
				{
					while (inLeft >= 4)
					{
						UTF8LEN_CHECKWORD(uint16_t, 0x8080, 2);
						UTF8LEN_CHECKWORD(uint16_t, 0x8080, 2);
					}
				}
				if (inLeft == 0) break;
			}
			else if ((cp & 0xe0) == 0xc0)
			{
				if (inLeft <= 2) break;
				++pIn; inLeft -= 2;
			}
			else if ((cp & 0xf0) == 0xe0)
			{
				if (inLeft <= 3) break;
				pIn += 2; inLeft -= 3;
			}
			else if ((cp & 0xf8) == 0xf0)
			{
				if (inLeft <= 4) break;
				pIn += 3; inLeft -= 4;
			} else
				return 0;
		}
	}

	return charsWritten;
}

#endif  // AML_USEASM

//----------------------------------------------------------------------------------------------------------------------
size_t UTF8To16(void* pDst, size_t dstLen, const char* pSrc, size_t srcLen)
{
	if (!pSrc) return 0;

	if (!pDst || !dstLen)
		return U8To16Len(pSrc, srcLen);

	const char* pIn = pSrc;
	size_t inLeft = srcLen;

	uint16_t* pOut = static_cast<uint16_t*>(pDst);
	uint16_t* pOutEnd = pOut + dstLen;

	if ((reinterpret_cast<uintptr_t>(pIn) & 3) == 0)
	{
		pOutEnd -= 4;
		for (uint32_t cp; inLeft >= 4; pOut += 4)
		{
			cp = *reinterpret_cast<const uint32_t*>(pIn);
			if (cp & 0x80808080) break;

			pIn += 4; inLeft -= 4;
			if (pOut > pOutEnd) { pOut += 4; goto GoOnCount; }

#if AML_LITTLE_ENDIAN
			pOut[0] = cp & 0xff; cp >>= 8; pOut[1] = cp & 0xff; cp >>= 8;
			pOut[2] = cp & 0xff; cp >>= 8; pOut[3] = static_cast<uint16_t>(cp);
#else
			pOut[3] = cp & 0xff; cp >>= 8; pOut[2] = cp & 0xff; cp >>= 8;
			pOut[1] = cp & 0xff; cp >>= 8; pOut[0] = static_cast<uint16_t>(cp);
#endif
		}
		pOutEnd += 4;
	}

	for (uint32_t cp; inLeft >= 2;)
	{
		for (;;)
		{
			cp = *pIn++;
			if (cp <= 0x7f)
			{
				--inLeft;
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = static_cast<uint16_t>(cp);

				if ((reinterpret_cast<uintptr_t>(pIn) & 1) == 0)
				{
					for (uint16_t v; inLeft >= 2; pOut += 2)
					{
						v = *reinterpret_cast<const uint16_t*>(pIn);
						if (v & 0x8080) break;

						pIn += 2; inLeft -= 2;
						if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
#if AML_LITTLE_ENDIAN
						pOut[0] = v & 0xff; pOut[1] = v >> 8;
#else
						pOut[0] = v >> 8; pOut[1] = v & 0xff;
#endif
					}
				}
			}
			else if ((cp & 0xe0) == 0xc0)
			{
				inLeft -= 2;
				uint8_t v = *pIn++;
				if ((v & 0xc0) != 0x80) return 0;
				cp = ((cp & 0x1f) << 6) | (v & 0x3f);
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = static_cast<uint16_t>(cp);
			} else
				break;
			if (inLeft < 2) goto LastByte;
		}

		size_t b = 2;
		if ((cp & 0xf0) == 0xe0) cp &= 0x0f;
		else if ((cp & 0xf8) == 0xf0) { cp &= 0x07; b = 3; }
		else return 0;
		if (inLeft < b) return 0;
		inLeft -= b + 1;
		do {
			uint8_t v = *pIn++;
			if ((v & 0xc0) != 0x80) return 0;
			cp = (cp << 6) | (v & 0x3f);
		} while (--b);
		if (cp <= 0xffff)
		{
			if ((cp >= 0xd800) && (cp <= 0xdfff)) return 0;
			if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
			*pOut++ = static_cast<uint16_t>(cp);
		} else
		{
			cp -= 0x10000;
			if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
			pOut[0] = static_cast<uint16_t>(0xd800 + (cp >> 10));
			pOut[1] = 0xdc00 + (cp & 0x3ff);
			pOut += 2;
		}
	}

LastByte:
	if (inLeft)
	{
		uint32_t cp = *pIn;
		if (cp > 0x7f) return 0;
		if (pOut < pOutEnd) *pOut = static_cast<uint16_t>(cp);
		++pOut;
	}
	return (reinterpret_cast<uintptr_t>(pOut) - reinterpret_cast<uintptr_t>(pDst)) >> 1;

GoOnCount:
	size_t res = U8To16Len(pIn, inLeft);
	if (res || !inLeft) res += (reinterpret_cast<uintptr_t>(pOut) - reinterpret_cast<uintptr_t>(pDst)) >> 1;
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UTF8To32(void* pDst, size_t dstLen, const char* pSrc, size_t srcLen)
{
	if (!pSrc) return 0;

	if (!pDst || !dstLen)
		return U8To32Len(pSrc, srcLen);

	const char* pIn = pSrc;
	size_t inLeft = srcLen;

	uint32_t* pOut = static_cast<uint32_t*>(pDst);
	uint32_t* pOutEnd = pOut + dstLen;

	if ((reinterpret_cast<uintptr_t>(pIn) & 3) == 0)
	{
		pOutEnd -= 4;
		for (uint32_t cp; inLeft >= 4; pOut += 4)
		{
			cp = *reinterpret_cast<const uint32_t*>(pIn);
			if (cp & 0x80808080) break;

			pIn += 4; inLeft -= 4;
			if (pOut > pOutEnd) { pOut += 4; goto GoOnCount; }

#if AML_LITTLE_ENDIAN
			pOut[0] = cp & 0xff; cp >>= 8; pOut[1] = cp & 0xff; cp >>= 8;
			pOut[2] = cp & 0xff; cp >>= 8; pOut[3] = cp;
#else
			pOut[3] = cp & 0xff; cp >>= 8; pOut[2] = cp & 0xff; cp >>= 8;
			pOut[1] = cp & 0xff; cp >>= 8; pOut[0] = cp;
#endif
		}
		pOutEnd += 4;
	}

	for (uint32_t cp; inLeft >= 2;)
	{
		for (;;)
		{
			cp = *pIn++;
			if (cp <= 0x7f)
			{
				--inLeft;
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = cp;

				if ((reinterpret_cast<uintptr_t>(pIn) & 1) == 0)
				{
					for (uint16_t v; inLeft >= 2; pOut += 2)
					{
						v = *reinterpret_cast<const uint16_t*>(pIn);
						if (v & 0x8080) break;

						pIn += 2; inLeft -= 2;
						if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
#if AML_LITTLE_ENDIAN
						pOut[0] = v & 0xff; pOut[1] = v >> 8;
#else
						pOut[0] = v >> 8; pOut[1] = v & 0xff;
#endif
					}
				}
			}
			else if ((cp & 0xe0) == 0xc0)
			{
				inLeft -= 2;
				uint8_t v = *pIn++;
				if ((v & 0xc0) != 0x80) return 0;
				cp = ((cp & 0x1f) << 6) | (v & 0x3f);
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = cp;
			} else
				break;
			if (inLeft < 2) goto LastByte;
		}

		size_t b = 2;
		if ((cp & 0xf0) == 0xe0) cp &= 0x0f;
		else if ((cp & 0xf8) == 0xf0) { cp &= 0x07; b = 3; }
		else return 0;
		if (inLeft < b) return 0;
		inLeft -= b + 1;
		do {
			uint8_t v = *pIn++;
			if ((v & 0xc0) != 0x80) return 0;
			cp = (cp << 6) | (v & 0x3f);
		} while (--b);
		if ((cp >= 0xd800) && (cp <= 0xdfff)) return 0;
		if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
		*pOut++ = cp;
	}

LastByte:
	if (inLeft)
	{
		uint32_t cp = *pIn;
		if (cp > 0x7f) return 0;
		if (pOut < pOutEnd) *pOut = cp;
		++pOut;
	}
	return (reinterpret_cast<uintptr_t>(pOut) - reinterpret_cast<uintptr_t>(pDst)) >> 2;

GoOnCount:
	size_t res = U8To32Len(pIn, inLeft);
	if (res || !inLeft) res += (reinterpret_cast<uintptr_t>(pOut) - reinterpret_cast<uintptr_t>(pDst)) >> 2;
	return res;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции конвертации UTF-16 и UTF-32 в UTF-8
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_USEASM

extern "C" {
	size_t AML_FASTCALL U16To8Len(const void* pStr, size_t size);
	size_t AML_FASTCALL U32To8Len(const void* pStr, size_t size);
}

#else

//----------------------------------------------------------------------------------------------------------------------
static size_t U16To8Len(const void* pStr, size_t size)
{
	const uint16_t* pIn = reinterpret_cast<const uint16_t*>(pStr);
	size_t inLeft = size, charsWritten = 0;

	for (uint32_t cp; inLeft;)
	{
		if ((reinterpret_cast<uintptr_t>(pIn) & 3) == 0)
		{
			while (inLeft >= 2)
			{
				cp = *reinterpret_cast<const uint32_t*>(pIn);
				if ((cp & 0xff80ff80) == 0)
				{
					pIn += 2; inLeft -= 2;
					charsWritten += 2;
					continue;
				}
				if (cp & 0xf800f800) break;
				pIn += 2; inLeft -= 2;
				charsWritten += 3;
				if ((cp & 0x0780) && (cp & 0x07800000))
					++charsWritten;
			}
			if (inLeft == 0) break;
		}

		cp = *pIn++; --inLeft;
		++charsWritten;

		if (cp <= 0x7f)
			continue;
		else if (cp <= 0x7ff)
			++charsWritten;
		else if ((cp < 0xd800) || (cp > 0xdfff))
			charsWritten += 2;
		else
		{
			if (!inLeft || (cp > 0xdbff)) return 0;
			if ((*pIn++ & 0xfc00) != 0xdc00) return 0;
			--inLeft; charsWritten += 3;
		}
	}

	return charsWritten;
}

//----------------------------------------------------------------------------------------------------------------------
static size_t U32To8Len(const void* pStr, size_t size)
{
	const uint32_t* pIn = reinterpret_cast<const uint32_t*>(pStr);
	size_t inLeft = size, charsWritten = 0;

	for (uint32_t cp; inLeft;)
	{
		if ((sizeof(size_t) == 8) && ((reinterpret_cast<uintptr_t>(pIn) & 7) == 0))
		{
			while (inLeft >= 2)
			{
				uint64_t cp64 = *reinterpret_cast<const uint64_t*>(pIn);
				if ((cp64 & 0xffffff80ffffff80) == 0)
				{
					pIn += 2; inLeft -= 2;
					charsWritten += 2;
					continue;
				}
				if (cp64 & 0xfffff800fffff800) break;
				pIn += 2; inLeft -= 2;
				charsWritten += 3;
				if ((cp64 & 0x0780) && (cp64 & 0x078000000000))
					++charsWritten;
			}
			if (inLeft == 0) break;
		}

		for (;;)
		{
			cp = *pIn++; --inLeft;
			++charsWritten;

			if (cp <= 0x7f)
				break;
			else if (cp <= 0x7ff)
			{
				++charsWritten;
				break;
			}
			else if (cp <= 0xffff)
				charsWritten += 2;
			else
				charsWritten += 3;
			if (inLeft == 0) break;
		}
	}

	return charsWritten;
}

#endif  // AML_USEASM

//----------------------------------------------------------------------------------------------------------------------
size_t UTF16To8(char* pDst, size_t dstLen, const void* pSrc, size_t srcLen)
{
	if (!pSrc) return 0;

	if (!pDst || !dstLen)
		return U16To8Len(pSrc, srcLen);

	const uint16_t* pIn = reinterpret_cast<const uint16_t*>(pSrc);
	size_t inLeft = srcLen;

	uint8_t* pOut = reinterpret_cast<uint8_t*>(pDst);
	uint8_t* pOutEnd = pOut + dstLen;

	while (inLeft)
	{
		uint32_t cp = *pIn++;
		--inLeft;

		for (;;)
		{
			if (cp <= 0x7f)
			{
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = static_cast<uint8_t>(cp);
				if ((reinterpret_cast<uintptr_t>(pIn) & (sizeof(size_t) - 1)) == 0) break;
				if (inLeft == 0) goto Exit;
				cp = *pIn++; --inLeft;
			}
			else if (cp <= 0x7ff)
			{
				if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
				pOut[0] = static_cast<uint8_t>(0xc0 | (cp >> 6));
				pOut[1] = 0x80 | (cp & 0x3f);
				pOut += 2;
				if (inLeft == 0) goto Exit;
				cp = *pIn++; --inLeft;
			} else
			{
				for (;;)
				{
					if ((cp >= 0xd800) && (cp <= 0xdfff))
					{
						if (!inLeft) return 0;
						if ((cp & 0xfc00) != 0xd800) return 0;
						uint32_t v = *pIn++; --inLeft;
						if ((v & 0xfc00) != 0xdc00) return 0;
						cp = (((cp & 0x3ff) << 10) | (v & 0x3ff)) + 0x10000;
					}
					if (cp <= 0xffff)
					{
						if (pOut + 2 >= pOutEnd) { pOut += 3; goto GoOnCount; }
						pOut[0] = static_cast<uint8_t>(0xe0 | (cp >> 12));
						pOut[1] = 0x80 | ((cp >> 6) & 0x3f);
						pOut[2] = 0x80 | (cp & 0x3f);
						pOut += 3;
					} else
					{
						if (pOut + 3 >= pOutEnd) { pOut += 4; goto GoOnCount; }
						pOut[0] = static_cast<uint8_t>(0xf0 | (cp >> 18));
						pOut[1] = 0x80 | ((cp >> 12) & 0x3f);
						pOut[2] = 0x80 | ((cp >> 6) & 0x3f);
						pOut[3] = 0x80 | (cp & 0x3f);
						pOut += 4;
					}
					if (inLeft == 0) goto Exit;
					cp = *pIn++; --inLeft;
					if (cp <= 0x7ff) break;
				}
			}
		}

		const size_t BPC = sizeof(size_t) / 2;
		for (size_t v; inLeft >= BPC; pOut += BPC)
		{
			v = *reinterpret_cast<const size_t*>(pIn);
			if (v & (0xff80ff80ff80ff80 & size_t(-1))) break;
			pIn += BPC; inLeft -= BPC;

			if (pOut + (BPC - 1) >= pOutEnd) { pOut += BPC; goto GoOnCount; }
			if (sizeof(size_t) == 4)
			{
#if AML_LITTLE_ENDIAN
				pOut[0] = v & 0xff; pOut[1] = static_cast<uint8_t>(v >> 16);
			} else {
				pOut[0] = v & 0xff; v >>= 16; pOut[1] = v & 0xff; v >>= 16;
				pOut[2] = v & 0xff; pOut[3] = static_cast<uint8_t>(v >> 16);
#else
				pOut[0] = static_cast<uint8_t>(v >> 16); pOut[1] = v & 0xff;
			} else {
				pOut[3] = v & 0xff; v >>= 16; pOut[2] = v & 0xff; v >>= 16;
				pOut[1] = v & 0xff; pOut[0] = static_cast<uint8_t>(v >> 16);
#endif
			}
		}
	}

Exit:
	return pOut - reinterpret_cast<uint8_t*>(pDst);

GoOnCount:
	size_t res = U16To8Len(pIn, inLeft);
	if (res || !inLeft) res += pOut - reinterpret_cast<uint8_t*>(pDst);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool UTF16To8(MemoryWriter& dst, const void* pSrc, size_t srcLen)
{
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
size_t UTF32To8(char* pDst, size_t dstLen, const void* pSrc, size_t srcLen)
{
	if (!pSrc) return 0;

	if (!pDst || !dstLen)
		return U32To8Len(pSrc, srcLen);

	const uint32_t* pIn = reinterpret_cast<const uint32_t*>(pSrc);
	size_t inLeft = srcLen;

	uint8_t* pOut = reinterpret_cast<uint8_t*>(pDst);
	uint8_t* pOutEnd = pOut + dstLen;

	while (inLeft)
	{
		uint32_t cp = *pIn++;
		--inLeft;

		for (;;)
		{
			if (cp <= 0x7f)
			{
				if (pOut >= pOutEnd) { ++pOut; goto GoOnCount; }
				*pOut++ = static_cast<uint8_t>(cp);
				if ((sizeof(size_t) == 8) && ((reinterpret_cast<uintptr_t>(pIn) & 7) == 0)) break;
				if (inLeft == 0) goto Exit;
				cp = *pIn++; --inLeft;
			}
			else if (cp <= 0x7ff)
			{
				if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
				pOut[0] = static_cast<uint8_t>(0xc0 | (cp >> 6));
				pOut[1] = 0x80 | (cp & 0x3f);
				pOut += 2;
				if (inLeft == 0) goto Exit;
				cp = *pIn++; --inLeft;
			} else
			{
				for (;;)
				{
					if (cp <= 0xffff)
					{
						if ((cp >= 0xd800) && (cp <= 0xdfff)) return 0;
						if (pOut + 2 >= pOutEnd) { pOut += 3; goto GoOnCount; }
						pOut[0] = static_cast<uint8_t>(0xe0 | (cp >> 12));
						pOut[1] = 0x80 | ((cp >> 6) & 0x3f);
						pOut[2] = 0x80 | (cp & 0x3f);
						pOut += 3;
					} else
					{
						if (pOut + 3 >= pOutEnd) { pOut += 4; goto GoOnCount; }
						pOut[0] = static_cast<uint8_t>(0xf0 | (cp >> 18));
						pOut[1] = 0x80 | ((cp >> 12) & 0x3f);
						pOut[2] = 0x80 | ((cp >> 6) & 0x3f);
						pOut[3] = 0x80 | (cp & 0x3f);
						pOut += 4;
					}
					if (inLeft == 0) goto Exit;
					cp = *pIn++; --inLeft;
					if (cp <= 0x7ff) break;
				}
			}
		}

		if (sizeof(size_t) == 8)
		{
			for (uint64_t v; inLeft >= 2; pOut += 2)
			{
				v = *reinterpret_cast<const size_t*>(pIn);
				if (v & 0xffffff80ffffff80) break;
				pIn += 2; inLeft -= 2;

				if (pOut + 1 >= pOutEnd) { pOut += 2; goto GoOnCount; }
#if AML_LITTLE_ENDIAN
				pOut[0] = v & 0xff; pOut[1] = static_cast<uint8_t>(v >> 32);
#else
				pOut[0] = static_cast<uint8_t>(v >> 32); pOut[1] = v & 0xff;
#endif
			}
		}
	}

Exit:
	return pOut - reinterpret_cast<uint8_t*>(pDst);

GoOnCount:
	size_t res = U32To8Len(pIn, inLeft);
	if (res || !inLeft) res += pOut - reinterpret_cast<uint8_t*>(pDst);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool UTF32To8(MemoryWriter& dst, const void* pSrc, size_t srcLen)
{
	return false;
}

} // namespace util
