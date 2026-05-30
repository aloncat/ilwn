//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "dbchunklist.h"

#include "dbchunk.h"

#include <core/toggle.h>

//--------------------------------------------------------------------------------------------------------------------------------
DBChunkList::DBChunkList()
{
	m_Chunks.reserve(30000);
	m_ToRemove.reserve(PURGE_GAIN);
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
	++m_ChunkCount;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBChunkList::Remove(DBChunk* chunk)
{
	EE::Assert(chunk, "Incorrect value");
	EE::Assert(!m_IsIterating, "Can't remove element while in ForEach");

	const size_t index = Find(chunk);
	EE::Assert(index != size_t(-1), "Element not found");

	// Принудительно устанавливаем состояние отсутствия несохранённых изменений, а затем
	// выгружаем все данные чанка. Так он будет занимать в памяти всего 40 байт (на x64)
	auto accessor = reinterpret_cast<DBChunkAccessor*>(chunk);
	accessor->SetSaveState(DBChunkState::UNCHANGED);
	chunk->UnloadData(DBChunkState::DATAUNLOADED);

	--m_ChunkCount;
	// Помещаем индекс чанка в список на удаление. Реальное освобождение памяти и удаление указателей из
	// m_Chunks сделаем при следующем вызове ForEach(), или когда накопится PURGE_GAIN удалённых чанков
	m_ToRemove.push_back(static_cast<uint32_t>(index));
	if (m_ToRemove.size() >= PURGE_GAIN)
		Purge();
}

//--------------------------------------------------------------------------------------------------------------------------------
int DBChunkList::ForEach(const std::function<int(DBChunk*)>& fn)
{
	if (!m_Chunks.empty() && fn)
	{
		EE::Assert(!m_IsIterating, "Recursion detected");
		util::Toggle<bool> lock(m_IsIterating, true);

		Purge();
		Sort();

		for (auto chunk : m_Chunks)
		{
			if (int retCode = fn(chunk))
				return retCode;
		}
	}

	return 0;
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
void DBChunkList::Purge()
{
	if (!m_ToRemove.empty())
	{
		size_t leftmost = size_t(-1);
		size_t rightmost = 0;

		auto chunks = m_Chunks.data();
		for (size_t index : m_ToRemove)
		{
			AML_SAFE_DELETE(chunks[index]);
			leftmost = std::min(leftmost, index);
			rightmost = std::max(rightmost, index);
		}

		for (size_t i = leftmost, j = i + 1; j < rightmost; ++j)
		{
			if (DBChunk* p = chunks[j])
				chunks[i++] = p;
		}

		auto it = m_Chunks.begin() + rightmost + 1;
		m_Chunks.erase(it - m_ToRemove.size(), it);
		m_ToRemove.clear();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t DBChunkList::Find(const DBChunk* chunk)
{
	if (const size_t total = m_Chunks.size())
	{
		Sort();

		auto chunks = m_Chunks.data();
		auto& first = chunk->GetFirst();
		size_t left = 0, right = total - 1;

		while (left < right)
		{
			size_t mid = (left + right) >> 1;
			if (chunks[mid]->GetFirst() < first)
				left = mid + 1;
			else
				right = mid;
		}

		for (size_t i = left; i < total; ++i)
		{
			if (chunks[i] == chunk)
				return i;
			if (chunks[i]->GetFirst() != first)
				break;
		}
	}

	return size_t(-1);
}
