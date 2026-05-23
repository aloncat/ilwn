//⬪AML⬪
#include "pch.h"
#include "memwriter.h"

#include "file.h"

#include <string.h>

using namespace util;

//----------------------------------------------------------------------------------------------------------------------
MemoryWriter::MemoryWriter(size_t blockSize)
	: m_Size(0)
	, m_BlockPos(0)
	, m_BlockSize(Clamp(blockSize, 1u, 64u) * 1024)
	, m_pLastBlock(&m_FirstBlock)
{
	m_FirstBlock.pNext = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
MemoryWriter::~MemoryWriter()
{
	Clear();
}

//----------------------------------------------------------------------------------------------------------------------
void MemoryWriter::Clear()
{
	m_Size = m_BlockPos = 0;
	m_pLastBlock = &m_FirstBlock;
	for (Block* pBlock = m_FirstBlock.pNext; pBlock;)
	{
		uint8_t* p = reinterpret_cast<uint8_t*>(pBlock);
		pBlock = pBlock->pNext;
		delete[] p;
	}
	m_FirstBlock.pNext = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
void MemoryWriter::Write(const void* pData, size_t bytesToWrite)
{
	size_t blockSize = (m_Size > LOCAL_SIZE) ? m_BlockSize : LOCAL_SIZE;
	if (bytesToWrite <= blockSize - m_BlockPos)
	{
		uint8_t* pOut = m_pLastBlock->data + m_BlockPos;

		#if !AML_DEBUG
			if (bytesToWrite < 16)
			{
				auto pIn = static_cast<const uint8_t*>(pData);
				for (size_t i = 0; i < bytesToWrite; ++i)
					pOut[i] = pIn[i];
			} else
				memcpy(pOut, pData, bytesToWrite);
		#else
			if (bytesToWrite)
				memcpy(pOut, pData, bytesToWrite);
		#endif

		m_Size += bytesToWrite;
		m_BlockPos += bytesToWrite;
	} else
		DoWrite(pData, bytesToWrite);
}

//----------------------------------------------------------------------------------------------------------------------
void MemoryWriter::SaveTo(MemoryWriter& dest, bool clearDest) const
{
	if (clearDest)
		dest.Clear();

	const Block* pBlock = &m_FirstBlock;
	for (size_t toCopy = LOCAL_SIZE; pBlock; toCopy = m_BlockSize)
	{
		if (!pBlock->pNext)
			toCopy = m_BlockPos;
		dest.Write(pBlock->data, toCopy);
		pBlock = pBlock->pNext;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryWriter::SaveTo(File& file, bool clearDest) const
{
	if (clearDest && !(file.SetPosition(0) && file.Truncate()))
		return false;

	const Block* pBlock = &m_FirstBlock;
	for (size_t toCopy = LOCAL_SIZE; pBlock; toCopy = m_BlockSize)
	{
		if (!pBlock->pNext)
			toCopy = m_BlockPos;
		if (!file.Write(pBlock->data, toCopy))
			return false;
		pBlock = pBlock->pNext;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryWriter::Grow()
{
	void* pNew = new uint8_t[sizeof(Block*) + m_BlockSize];
	Block* pNewBlock = static_cast<Block*>(pNew);
	pNewBlock->pNext = nullptr;

	m_BlockPos = 0;
	m_pLastBlock->pNext = pNewBlock;
	m_pLastBlock = pNewBlock;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryWriter::DoWrite(const void* pData, size_t bytesToWrite)
{
	auto pIn = static_cast<const uint8_t*>(pData);
	size_t blockSize = (m_Size > LOCAL_SIZE) ? m_BlockSize : LOCAL_SIZE;
	for (size_t bytesLeft = blockSize - m_BlockPos;; bytesLeft = m_BlockSize)
	{
		const size_t toWrite = (bytesToWrite <= bytesLeft) ? bytesToWrite : bytesLeft;
		memcpy(m_pLastBlock->data + m_BlockPos, pIn, toWrite);

		m_Size += toWrite;
		m_BlockPos += toWrite;
		bytesToWrite -= toWrite;
		if (!bytesToWrite)
			break;

		pIn += toWrite;
		Grow();
	}
}
