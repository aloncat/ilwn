//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "assert.h"

#include <core/util.h>

#include <functional>
#include <vector>

class DBChunk;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunkList - контейнер для объектов DBChunk
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class DBChunkList final
{
	AML_NONCOPYABLE(DBChunkList)

public:
	DBChunkList();
	~DBChunkList();

	void Insert(DBChunk* chunk);
	void Remove(DBChunk* chunk);

	// Вызывает пользовательскую функцию fn для каждого объекта в контейнере. Гарантируется, что объекты
	// будут перечисляться по возрастанию их значений GetFirst(). Функция fn должна вернуть любое число,
	// кроме 0, если обход контейнера нужно прекратить, или вернуть 0, если нужно продолжить. После
	// завершения работы ForEach() вернёт значение, возвращённое пользовательской функцией
	int ForEach(const std::function<int(DBChunk*)>& fn);

	// Возвращает количество объектов в контейнере
	size_t GetSize() const { return m_ChunkCount; }

private:
	// Количество помеченных к удалению чанков,
	// после которого они удаляются принудительно
	static constexpr size_t PURGE_GAIN = 8000;

	// EE::Assert и EE::Verify генерируют при ошибках исключение EDBLogic
	// и предназначены для контроля корректности использования класса
	using EE = AssertHelper<EDBLogic>;

	void Sort();
	void Purge();

	size_t Find(const DBChunk* chunk);

private:
	std::vector<DBChunk*> m_Chunks;		// Все чанки, включая указатели удалённых чанков
	std::vector<uint32_t> m_ToRemove;	// Список индексов элементов в m_Chunks для удаления
	size_t m_ChunkCount = 0;			// Количество действительных элементов в m_Chunks

	bool m_IsIterating = false;
	bool m_IsSorted = true;
};
