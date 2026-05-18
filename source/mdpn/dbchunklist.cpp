//∙MDPN
#include "pch.h"
#include "dbchunklist.h"

#include "dbchunk.h"

#include <core/toggle.h>

//----------------------------------------------------------------------------------------------------------------------
DBChunkList::DBChunkList()
{
	m_Chunks.reserve(4096);
}

//----------------------------------------------------------------------------------------------------------------------
DBChunkList::~DBChunkList()
{
	for (auto& p : m_Chunks)
		AML_SAFE_DELETE(p);
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkList::Insert(DBChunk* pChunk)
{
	EE::Assert(pChunk, "Incorrect value");
	EE::Assert(!m_IsIterating, "Can't insert element while in ForEach");
	EE::Assert(!pChunk->HasOwner(), "Element already has an owner");

	if (m_IsSorted && !m_Chunks.empty())
	{
		if (pChunk->GetDataState() < DBChunkState::DATAUNLOADED || pChunk->GetFirst() < m_Chunks.back()->GetFirst())
			m_IsSorted = false;
	}

	m_Chunks.push_back(pChunk);
	pChunk->SetHasOwner();
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkList::Remove(DBChunk* pChunk)
{
	EE::Assert(pChunk, "Incorrect value");
	EE::Assert(!m_IsIterating, "Can't remove element while in ForEach");

	auto it = Find(pChunk);
	EE::Assert(it != m_Chunks.end(), "Element not found");

	m_Chunks.erase(it);
	delete pChunk;
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkList::ForEach(const std::function<bool(DBChunk*)>& fn)
{
	if (!m_Chunks.empty() && fn)
	{
		EE::Assert(!m_IsIterating, "Recursion detected");
		util::Toggle<bool> lock(m_IsIterating, true);

		Sort();
		for (auto p : m_Chunks)
		{
			if (!fn(p))
				break;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DBChunkList::Sort()
{
	if (!m_IsSorted)
	{
		// NB: в режиме безопасной загрузки БД (при вызове DataBase::SafeInit), некоторые файлы могут быть
		// незагружены. Такие файлы будут иметь состояние DBChunkState::FILEPATHONLY. Для них будем считать
		// значение GetFirst() равным 0. Все эти файлы окажутся в начале списка файлов после сортировки
		std::sort(m_Chunks.begin(), m_Chunks.end(), [](const DBChunk* pLhs, const DBChunk* pRhs) {
			if (pLhs->GetDataState() < DBChunkState::DATAUNLOADED)
				return pRhs->GetDataState() >= DBChunkState::DATAUNLOADED;
			else if (pRhs->GetDataState() >= DBChunkState::DATAUNLOADED)
				return pLhs->GetFirst() < pRhs->GetFirst();
			return false;
		});
		m_IsSorted = true;
	}
}

//----------------------------------------------------------------------------------------------------------------------
std::vector<DBChunk*>::iterator DBChunkList::Find(const DBChunk* pChunk)
{
	if (const size_t totalC = m_Chunks.size())
	{
		Sort();
		auto& first = pChunk->GetFirst();
		size_t left = 0, right = totalC - 1;
		while (left < right)
		{
			size_t mid = (left + right) >> 1;
			if (m_Chunks[mid]->GetFirst() < first)
				left = mid + 1;
			else
				right = mid;
		}
		for (size_t i = left; i < totalC; ++i)
		{
			if (m_Chunks[i] == pChunk)
				return m_Chunks.begin() + i;
			if (m_Chunks[i]->GetFirst() != first)
				break;
		}
	}
	return m_Chunks.end();
}
