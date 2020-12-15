//⬪MDPN⬪
#pragma once

#include "assert.h"

#include <core/util.h>

#include <functional>
#include <vector>

class DBChunk;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DBChunkList - контейнер для объектов DBChunk
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class DBChunkList final
{
	AML_NONCOPYABLE(DBChunkList)

public:
	DBChunkList();
	~DBChunkList();

	void Insert(DBChunk* pChunk);
	void Remove(DBChunk* pChunk);

	// Вызывает пользовательскую функцию fn для каждого объекта в контейнере. Гарантируется, что объекты
	// будут перечисляться по возрастанию их значений GetFirst(). Функция fn должна вернуть false, если
	// обход контейнера нужно прекратить, или вернуть true, если нужно продолжить
	void ForEach(const std::function<bool(DBChunk*)>& fn);

	// Возвращает количество объектов в контейнере
	size_t GetSize() const { return m_Chunks.size(); }

private:
	// EE::Assert и EE::Verify генерируют при ошибках исключение EDBLogic
	// и предназначены для контроля корректности использования класса
	using EE = AssertHelper<EDBLogic>;

	void Sort();
	std::vector<DBChunk*>::iterator Find(const DBChunk* pChunk);

	std::vector<DBChunk*> m_Chunks;
	bool m_IsIterating = false;
	bool m_IsSorted = true;
};
