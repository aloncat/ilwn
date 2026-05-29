//∙MDPN

#include "pch.h"
#include "dbchunklist.h"

#include "dbchunk.h"

#include <core/toggle.h>

//--------------------------------------------------------------------------------------------------------------------------------
DBChunkList::DBChunkList()
{
	m_Chunks.reserve(4096);
}

//--------------------------------------------------------------------------------------------------------------------------------
DBChunkList::~DBChunkList()
{
	for (auto& chunk : m_Chunks)
		AML_SAFE_DELETE(chunk);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBChunkList::Insert(DBChunk* chunk)
{
	EE::Assert(chunk, "Incorrect value");
	EE::Assert(!m_IsIterating, "Can't insert element while in ForEach");
	EE::Assert(!chunk->HasOwner(), "Element already has an owner");

	if (m_IsSorted && !m_Chunks.empty())
	{
		if (chunk->GetDataState() < DBChunkState::DATAUNLOADED || chunk->GetFirst() < m_Chunks.back()->GetFirst())
			m_IsSorted = false;
	}

	m_Chunks.push_back(chunk);
	chunk->SetHasOwner();
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBChunkList::Remove(DBChunk* chunk)
{
	EE::Assert(chunk, "Incorrect value");
	EE::Assert(!m_IsIterating, "Can't remove element while in ForEach");

	auto it = Find(chunk);
	EE::Assert(it != m_Chunks.end(), "Element not found");

	m_Chunks.erase(it);
	delete chunk;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBChunkList::ForEach(const std::function<bool(DBChunk*)>& fn)
{
	if (!m_Chunks.empty() && fn)
	{
		EE::Assert(!m_IsIterating, "Recursion detected");
		util::Toggle<bool> lock(m_IsIterating, true);

		Sort();
		for (auto chunk : m_Chunks)
		{
			if (!fn(chunk))
				break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBChunkList::Sort()
{
	if (!m_IsSorted)
	{
		// В режиме безопасной загрузки БД (при вызове DataBase::SafeInit), некоторые файлы могут быть
		// незагружены. Такие файлы будут иметь состояние DBChunkState::FILEPATHONLY. Для них будем считать
		// значение GetFirst() равным 0. Все эти файлы окажутся в начале списка файлов после сортировки
		std::sort(m_Chunks.begin(), m_Chunks.end(), [](const DBChunk* lhs, const DBChunk* rhs) {
			if (lhs->GetDataState() < DBChunkState::DATAUNLOADED)
				return rhs->GetDataState() >= DBChunkState::DATAUNLOADED;
			else if (rhs->GetDataState() >= DBChunkState::DATAUNLOADED)
				return lhs->GetFirst() < rhs->GetFirst();
			return false;
		});
		m_IsSorted = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
std::vector<DBChunk*>::iterator DBChunkList::Find(const DBChunk* chunk)
{
	if (const size_t total = m_Chunks.size())
	{
		Sort();

		auto& first = chunk->GetFirst();
		size_t left = 0, right = total - 1;

		while (left < right)
		{
			size_t mid = (left + right) >> 1;
			if (m_Chunks[mid]->GetFirst() < first)
				left = mid + 1;
			else
				right = mid;
		}

		for (size_t i = left; i < total; ++i)
		{
			if (m_Chunks[i] == chunk)
				return m_Chunks.begin() + i;
			if (m_Chunks[i]->GetFirst() != first)
				break;
		}
	}

	return m_Chunks.end();
}
