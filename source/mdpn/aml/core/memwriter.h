//⬪AML⬪
#pragma once

#include "forward.h"
#include "platform.h"
#include "util.h"

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class MemoryWriter
{
	AML_NONCOPYABLE(MemoryWriter)

public:
	// Параметр blockSize задаёт размер блока в килобайтах. Все записываемые в контейнер данные хранятся
	// в связном списке таких блоков. Когда в текущем блоке заканчивается свободное место, выделяется
	// память под новый блок, он становится текущим и добавляется в конец списка
	explicit MemoryWriter(size_t blockSize = 8);
	~MemoryWriter();

	void Clear();

	// Возвращает количество записанных байт
	size_t GetSize() const { return m_Size; }

	// Записывает bytesToWrite байт из pData в контейнер
	void Write(const void* pData, size_t bytesToWrite);

	// Копирует все данные из этого контейнера в другой контейнер или файл. Если clearDest равен false,
	// то данные дописываются в контейнер dest (или в файл file, начиная с текущей позиции). Если же
	// clearDest равен true, то контейнер dest (или файл file) будет очищен перед копированием
	void SaveTo(MemoryWriter& dest, bool clearDest = false) const;
	bool SaveTo(File& file, bool clearDest = false) const;

protected:
	// Размер первого блока списка в байтах. Этот блок расположен в данных класса
	// (член m_FirstBlock), а не выделяется в куче, как следующие за ним блоки
	static const size_t LOCAL_SIZE = 512;

	struct Block {
		Block* pNext;
		uint8_t data[LOCAL_SIZE];
	};

	void Grow();
	void DoWrite(const void* pData, size_t bytesToWrite);

	size_t m_Size;				// Суммарный размер данных
	size_t m_BlockPos;			// Текущая позиция в текущем блоке
	const size_t m_BlockSize;	// Размер блока в байтах (кроме первого)
	Block* m_pLastBlock;		// Указатель на текущий (последний) блок
	Block m_FirstBlock;			// Первый блок списка
};

} // namespace util
