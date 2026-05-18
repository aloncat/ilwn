//⬪AML⬪
#include "pch.h"
#include "fasthash.h"

namespace hash {

// Функции GetFastHash используют алгоритм FNV-1a (32-битный хеш), который не уступает по скорости работы
// алгоритму SDBM. Но в отличие от SDBM, FNV-1a дает лучшее распределение. Среди простых мультипликативных
// хеш-функций FNV-1a является одной из лучших. Но её использование целесообразно лишь для коротких строк

static const uint32_t FNV_SEED = 0x811c9dc5;
static const uint32_t FNV_PRIME = 0x01000193;

#define FNV_HASH_1B(V) hash = (hash ^ (V)) * FNV_PRIME
#define FNV_HASH_2B(V) { FNV_HASH_1B((V) & 0xff); FNV_HASH_1B((V) >> 8); }
#define FNV_HASH_4B(V) { FNV_HASH_2B((V) & 0xffff); V >>= 16; FNV_HASH_2B(V); }

static const uint8_t loCaseTTA[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32 };

//----------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const char* pStr, bool toLower)
{
	uint32_t v, hash = FNV_SEED;
	if (const uint8_t* p = reinterpret_cast<const uint8_t*>(pStr))
	{
		if (toLower)
		{
			while ((v = *p++) != 0)
			{
				v += loCaseTTA[v];
				FNV_HASH_1B(v);
			}
		} else
		{
			while ((v = *p++) != 0)
				FNV_HASH_1B(v);
		}
	}
	return hash;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const wchar_t* pStr, bool toLower)
{
	uint32_t v, hash = FNV_SEED;
	if (sizeof(wchar_t) == 2)
	{
		if (const uint16_t* p = reinterpret_cast<const uint16_t*>(pStr))
		{
			if (toLower)
			{
				while ((v = *p++) != 0)
				{
					if (v <= 0x7f)
						v += loCaseTTA[v];
					FNV_HASH_2B(v);
				}
			} else
			{
				while ((v = *p++) != 0)
					FNV_HASH_2B(v);
			}
		}
	}
	else if (const uint32_t* p = reinterpret_cast<const uint32_t*>(pStr))
	{
		if (toLower)
		{
			while ((v = *p++) != 0)
			{
				if (v <= 0x7f)
					v += loCaseTTA[v];
				FNV_HASH_4B(v);
			}
		} else
		{
			while ((v = *p++) != 0)
				FNV_HASH_4B(v);
		}
	}
	return hash;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const uint16_t* pStr)
{
	uint32_t v, hash = FNV_SEED;
	if (pStr)
	{
		while ((v = *pStr++) != 0)
			FNV_HASH_2B(v);
	}
	return hash;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const uint16_t* pStr, size_t count)
{
	uint32_t v, hash = FNV_SEED;
	while (count--)
	{
		v = *pStr++;
		FNV_HASH_2B(v);
	}
	return hash;
}

//----------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const void* pData, size_t size, unsigned prevHash)
{
	uint32_t hash = prevHash;
	const uint8_t* p = reinterpret_cast<const uint8_t*>(pData);
	while (size--)
		FNV_HASH_1B(*p++);
	return hash;
}

} // namespace hash
