//∙MDPN
#include "pch.h"
#include "numset.h"

#include "largemempages.h"

#include <core/exception.h>
#include <core/winapi.h>

//----------------------------------------------------------------------------------------------------------------------
NumberSet::NumberSet(bool useLargePages)
	: m_HBits(HASH_BITS - 3)
{
	static_assert(CLEAR_GAIN >= 2 && CLEAR_GAIN < EIGHTH_CHUNK_C, "Incorrect CLEAR_GAIN value");
	// В целях увеличения скорости работы функции Purge за выбор части отвечают биты 12-14. Ещё 3
	// бита нужны для роста таблицы. Таким образом, длина хеша должна быть не менее 12+3+3=18 бит
	static_assert(HASH_BITS >= 18 && HASH_BITS <= 30, "HASH_BITS must be >= 18");

	if (useLargePages)
	{
		const size_t pageSize = LargeMemPages::GetLargePageSize();
		m_LPageSize = LargeMemPages::IsEnabled() ? pageSize : 0;
		m_HBits = m_LPageSize ? HASH_BITS : HASH_BITS - 3;
	}

	// Выделяем память под весь массив без инициализации элементов
	m_TableA = AllocateMem(size_t(1) << HASH_BITS);
	// Инициализируем 1/8 часть элементов массива
	const size_t itemC = size_t(1) << m_HBits;
	for (size_t i = 0; i < itemC; ++i)
		m_TableA[i].Reset();

	m_ChunkA = new Item*[8 * EIGHTH_CHUNK_C];
	AML_FILLA(m_ChunkA, 0, 8 * EIGHTH_CHUNK_C);

	for (int i = 0; i < 8; ++i)
		m_NextA[i] = 0;
	for (int i = 0; i < 8; ++i)
		m_RCountA[i] = 0;
}

//----------------------------------------------------------------------------------------------------------------------
NumberSet::~NumberSet()
{
	for (size_t i = 0; i < 8 * EIGHTH_CHUNK_C; ++i)
		FreeMem(m_ChunkA[i]);
	delete[] m_ChunkA;
	FreeMem(m_TableA);
}

//----------------------------------------------------------------------------------------------------------------------
void NumberSet::Clear(bool freeMem)
{
	for (size_t eighth = 0; eighth < 8; ++eighth)
	{
		Item** eighthChunkA = &m_ChunkA[eighth * EIGHTH_CHUNK_C];
		for (size_t i = freeMem ? 0 : CLEAR_GAIN; i < EIGHTH_CHUNK_C; ++i)
			FreeMem(eighthChunkA[i]);
	}
	if (freeMem && !m_LPageSize)
	{
		FreeMem(m_TableA);
		m_HBits = HASH_BITS - 3;
		m_TableA = AllocateMem(size_t(1) << HASH_BITS);
	}

	const size_t itemC = size_t(1) << m_HBits;
	for (size_t i = 0; i < itemC; ++i)
		m_TableA[i].Reset();
	for (int i = 0; i < 8; ++i)
		m_NextA[i] = 0;
	for (int i = 0; i < 8; ++i)
		m_RCountA[i] = 0;
	m_TCount = m_CCount = m_RCount = 0;
	m_Purge = 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool NumberSet::Exists(const Number& num) const
{
	const Item* p = &m_TableA[GetHash(num)];
	if (p->num.IsZero())
		return false;

	for (; p->num != num; p = GetItem(p->next))
	{
		if (p->next == Item::LAST)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool NumberSet::Exists(const FixNumber& num) const
{
	const Item* p = &m_TableA[GetHash(num)];
	if (p->num.IsZero())
		return false;

	for (; p->num != num; p = GetItem(p->next))
	{
		if (p->next == Item::LAST)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool NumberSet::Insert(const FixNumber& num)
{
	if (num.IsZero())
		return true;

	// Ограничение на суммарное количество элементов - не более, чем 8 * CLEAR_GAIN полных блоков
	if (m_CCount >= 8 * CLEAR_GAIN * CHUNK_SIZE)
	{
		size_t eighth = 0;
		size_t maxC = m_NextA[0];
		for (size_t i = 1; i < 8; ++i)
		{
			if (m_NextA[i] > maxC)
			{
				maxC = m_NextA[i];
				eighth = i;
			}
		}
		Purge(eighth);
	}

	// Если использовано более 1/2 элементов хеш-таблицы, но задействованы
	// ещё не все биты хеша, то увеличиваем размер её используемой части
	if (m_HBits < HASH_BITS && m_TCount > size_t(1) << (m_HBits - 1))
		RaiseTable();

	const unsigned hash = GetHash(num);
	Item* p = &m_TableA[hash];
	if (p->num.IsZero())
	{
		++m_TCount;
		p->num = num;
		return true;
	}

	const size_t eighth = (hash >> 12) & 7;
	if (m_Purge & (1 << eighth))
		Purge(eighth);

	for (; p->num != num; p = GetItem(p->next))
	{
		if (p->next == Item::LAST)
		{
			p->next = GetNext(eighth);
			p = GetItem(p->next);
			p->num = num;
			p->next = Item::LAST;
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void NumberSet::RaiseTable()
{
	if (m_HBits == HASH_BITS)
		return;

	// Инициализируем новые задействуемые элементы
	const size_t count = size_t(1) << m_HBits++;
	for (size_t i = count; i < count << 1; ++i)
		m_TableA[i].Reset();

	// Пробегаемся по всем задействованным элементам таблицы
	// и разносим все числа их цепочек по 2 разным хешам
	for (size_t i = 0; i < count; ++i)
	{
		// Если число в таблице равно 0, то значит
		// цепи элементов нет, пропускаем этот хеш
		Item* pTail1 = &m_TableA[i];
		if (pTail1->num.IsZero())
			continue;

		Item* pTail2 = pTail1 + count;
		if (GetHash(pTail1->num) != i)
		{
			pTail2->num = pTail1->num;
			pTail1->num.SetZero();
		}
		uint32_t current = pTail1->next;
		pTail1->next = Item::LAST;

		while (current != Item::LAST)
		{
			Item* p = GetItem(current);
			Item*& pTail = (GetHash(p->num) == i) ? pTail1 : pTail2;
			if (pTail->num.IsZero())
			{
				++m_TCount;
				++m_RCount;
				++m_RCountA[(i >> 12) & 7];
				pTail->num = p->num;
			} else
			{
				pTail->next = current;
				pTail = p;
			}
			current = p->next;
			p->next = Item::LAST;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void NumberSet::Purge(size_t eighth)
{
	// Обходим все числа первого блока сокращаемой части eighth, именно этот блок чисел
	// нам будет нужно удалить. Для каждого элемента ищем в его цепочке конечный элемент
	// или элемент, ссылающийся на число в другом блоке, и заменяем им текущий элемент
	const size_t last = (eighth * EIGHTH_CHUNK_C + 1) * CHUNK_SIZE;
	Item* pChunk = m_ChunkA[eighth * EIGHTH_CHUNK_C];
	for (size_t i = 0; i < CHUNK_SIZE; ++i)
	{
		Item* p = &pChunk[i];
		while (p->next < last)
			p = &pChunk[p->next & (CHUNK_SIZE - 1)];
		pChunk[i] = *p;
	}
	// Теперь обходим все элементы хеш-таблицы, соответствующие сокращаемой части eighth. Если элемент
	// конечный, то мы ничего не меняем. Если он ссылается на число из первого блока, то копируем это
	// число из блока в элемент таблицы. Затем корректируем в элементе ссылку на следующий элемент
	const size_t itemC = size_t(1) << (m_HBits - 15);
	for (size_t i = 0; i < itemC; ++i)
	{
		Item* tabA = &m_TableA[(8 * i + eighth) << 12];
		for (size_t j = 0; j < 1 << 12; ++j)
		{
			uint32_t next = tabA[j].next;
			if (next < last)
			{
				Item* p = &pChunk[next & (CHUNK_SIZE - 1)];
				tabA[j].num = p->num;
				next = p->next;
			}
			uint32_t n = next - CHUNK_SIZE;
			tabA[j].next = (next == Item::LAST) ? next : n;
		}
	}
	// Теперь корректируем ссылки в остальных блоках
	size_t totalC = m_NextA[eighth];
	for (size_t chunk = 1; totalC > CHUNK_SIZE; ++chunk)
	{
		totalC -= CHUNK_SIZE;
		pChunk = m_ChunkA[eighth * EIGHTH_CHUNK_C + chunk];
		size_t chunkSize = (totalC < CHUNK_SIZE) ? totalC : CHUNK_SIZE;
		for (size_t i = 0; i < chunkSize; ++i)
		{
			uint32_t next = pChunk[i].next;
			uint32_t n = next - CHUNK_SIZE;
			pChunk[i].next = (next == Item::LAST) ? next : n;
		}
	}

	m_CCount -= CHUNK_SIZE;
	m_NextA[eighth] -= CHUNK_SIZE;
	m_Purge &= ~(1 << eighth);

	// Если счётчик RCount для этой части был >0, то после удаления блока все
	// её перенесённые элементы будут освобождены и счётчик нужно обнулить
	m_RCount -= m_RCountA[eighth];
	m_RCountA[eighth] = 0;

	// Если часть использовала CLEAR_GAIN + 1 блоков или меньше, то помещаем удалённый
	// блок за последним используемым. Если блоков было больше, то освобождаем память
	Item** chunkA = &m_ChunkA[eighth * EIGHTH_CHUNK_C];
	if (CLEAR_GAIN + 1 < EIGHTH_CHUNK_C && chunkA[CLEAR_GAIN + 1])
	FreeMem(chunkA[0]);

	Item* pSpareBlock = chunkA[0];
	// Сдвигаем все оставшиеся блоки к началу
	for (size_t i = 1; i < EIGHTH_CHUNK_C; ++i)
	{
		chunkA[i - 1] = chunkA[i];
		if (!chunkA[i])
		{
			chunkA[i - 1] = pSpareBlock;
			pSpareBlock = nullptr;
			break;
		}
	}
	chunkA[EIGHTH_CHUNK_C - 1] = pSpareBlock;
}

//----------------------------------------------------------------------------------------------------------------------
uint32_t NumberSet::GetNext(size_t eighth)
{
	size_t next = m_NextA[eighth]++;
	++m_CCount;

	Item*& pChunk = m_ChunkA[next / CHUNK_SIZE + eighth * EIGHTH_CHUNK_C];
	if (!pChunk)
		pChunk = AllocateMem(CHUNK_SIZE);

	if (next >= EIGHTH_CHUNK_C * CHUNK_SIZE - 1)
		m_Purge |= 1 << eighth;

	next += eighth * EIGHTH_CHUNK_C * CHUNK_SIZE;
	return static_cast<uint32_t>(next);
}

//----------------------------------------------------------------------------------------------------------------------
template<class T> inline unsigned NumberSet::GetHash(const T& num) const
{
	const unsigned hash = num.GetHash();
	const unsigned hashMask = ~(~0 << m_HBits);
	return (hash ^ (hash >> HASH_BITS)) & hashMask;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE NumberSet::Item* NumberSet::AllocateMem(size_t itemC)
{
	const size_t sizeInBytes = sizeof(Item) * itemC;
	// Для выделения памяти большими страницами размер sizeInBytes должен
	// быть кратен размеру большой страницы, которая обычно равна 2048KiB
	if (m_LPageSize && !(sizeInBytes & (m_LPageSize - 1)))
	{
		if (void* p = ::VirtualAlloc(nullptr, sizeInBytes, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE))
			return reinterpret_cast<Item*>(p);
	}
	if (void* p = ::VirtualAlloc(nullptr, sizeInBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))
		return reinterpret_cast<Item*>(p);

	throw util::ERuntime("Failed to allocate memory");
}

//----------------------------------------------------------------------------------------------------------------------
void NumberSet::FreeMem(Item*& pBlock)
{
	if (pBlock)
	{
		::VirtualFree(pBlock, 0, MEM_RELEASE);
		pBlock = nullptr;
	}
}
