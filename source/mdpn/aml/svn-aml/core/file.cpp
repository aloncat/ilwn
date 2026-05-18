#include "file.h"

#include "../../core/crc32.h"

using namespace util;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BufferedFile
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
BufferedFile::BufferedFile(size_t bufferSize)
	: m_pBuffer(0)
	, m_BufferSize(Clamp(bufferSize, 4u, 512u) * 1024)
{
}

//----------------------------------------------------------------------------------------------------------------------
BufferedFile::~BufferedFile()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------
void BufferedFile::Close()
{
	if (m_pBuffer)
	{
		FlushBuffer();
		free(m_pBuffer);
		m_pBuffer = 0;
	}
	BinaryFile::Close();
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Flush()
{
	return FlushBuffer() && BinaryFile::Flush();
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::FlushBuffer()
{
	if (!m_pBuffer) return false;
	// Позиция m_BufferPos, отличная от 0, может означать, что в буфере есть записанные данные.
	// Но если при этом m_CachedSize не равен 0, то значит буфер находится в режиме чтения.
	return !m_BufferPos || m_CachedSize || WriteCache();
}

//----------------------------------------------------------------------------------------------------------------------
long long BufferedFile::GetPosition() const
{
	if (!m_pBuffer) return -1;
	// Если в буфере есть прочитанные данные, то независимо от реальной позиции файла
	// (не важно, чему равен m_SeekError), возвращаем текущую позицию чтения в буфере.
	if (m_CachedSize) return m_FilePos - m_CachedSize + m_BufferPos;
	// Если не было ошибки позиционирования (т.е. m_FilePos совпадает с реальной позицией файла) или
	// в буфере есть записанные данные, то возвращаем известную нам позицию. В случае наличия ошибки
	// позиционирования и данных в буфере записи следующей операцией с файлом станет сохранение
	// данных из буфера, при котором известная позиция станет реальной позицией файла.
	if (!m_SeekError || m_BufferPos) return m_FilePos + m_BufferPos;

	// Если же была ошибка и данных в буфере нет, попытаемся выяснить реальную
	// текущую позицию файла. В случае успеха запомним ее и сбросим ошибку.
	long long pos = BinaryFile::GetPosition();
	if (pos >= 0)
	{
		m_FilePos = pos;
		m_SeekError = false;
	}
	return pos;
}

//----------------------------------------------------------------------------------------------------------------------
long long BufferedFile::GetSize() const
{
	long long size = BinaryFile::GetSize();
	// Если в буфере есть записанные данные, то они могут увеличить размер файла,
	// если находятся за концом имеющихся на текущий момент данных в файле.
	if (m_BufferPos && !m_CachedSize && (size >= 0))
	{
		long long position = m_FilePos + m_BufferPos;
		if (position >= size) return position;
	}
	return size;
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Open(const std::wstring& path, unsigned flags)
{
	if (BinaryFile::Open(path, flags))
	{
		m_BufferPos = 0;
		m_CachedSize = 0;
		m_FilePos = 0;
		m_SeekError = false;

		m_pBuffer = static_cast<uint8_t*>(malloc(m_BufferSize));
		if (m_pBuffer) return true;

		BinaryFile::Close();
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
inline void BufferedFile::PurgeCache()
{
	size_t bytesLeft = m_CachedSize - m_BufferPos;
	m_BufferPos = m_CachedSize = 0;

	m_FilePos -= bytesLeft;
	m_SeekError |= (bytesLeft != 0);
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead)
{
	bytesRead = 0;

	if (!m_pBuffer) return false;
	if (!bytesToRead) return true;
	if (!pBuffer) return false;

	if (!m_CachedSize && m_BufferPos && !WriteCache())
		return false;

	uint8_t* pOut = static_cast<uint8_t*>(pBuffer);
	for (bool secondPass = false;; secondPass = true)
	{
		size_t bytesLeft = m_CachedSize - m_BufferPos;
		size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead : bytesLeft;

		const uint8_t* pIn = &m_pBuffer[m_BufferPos];
#if !AML_DEBUG
		if (toRead < 16)
		{
			for (size_t i = 0; i < toRead; ++i)
				pOut[i] = pIn[i];
		} else
			memcpy(pOut, pIn, toRead);
#else
		if (toRead)
			memcpy(pOut, pIn, toRead);
#endif
		m_BufferPos += toRead;
		bytesRead += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead || secondPass)
			return true;

		pOut += toRead;
		m_BufferPos = m_CachedSize = 0;

		size_t bytesActuallyRead = 0;
		if (bytesToRead >= m_BufferSize)
		{
			bool res = ReadFromFile(pOut, bytesToRead, bytesActuallyRead);
			if (res) bytesRead += bytesActuallyRead;
			return res;
		}
		if (!ReadFromFile(m_pBuffer, m_BufferSize, bytesActuallyRead))
			return false;
		m_CachedSize = bytesActuallyRead;
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool BufferedFile::ReadFromFile(void* pBuffer, size_t bytesToRead, size_t& bytesRead)
{
	if (!m_SeekError || BinaryFile::SetPosition(m_FilePos))
	{
		m_SeekError = true;
		if (BinaryFile::Read(pBuffer, bytesToRead, bytesRead))
		{
			m_FilePos += bytesRead;
			m_SeekError = false;
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::SetPosition(long long position)
{
	if (!m_pBuffer || (position < 0)) return false;
	if (m_BufferPos && !m_CachedSize && !WriteCache())
		return false;

	// Если в буфере есть прочитанные данные и новая позиция остается в пределах буфера,
	// то мы можем изменить только m_BufferPos, не меняя реальную текущую позицию файла.
	if (m_CachedSize && (position < m_FilePos) && (position >= static_cast<long long>(m_FilePos - m_CachedSize)))
		m_BufferPos = static_cast<size_t>(position - m_FilePos + m_CachedSize);
	else
	{
		m_BufferPos = 0;
		m_CachedSize = 0;
		m_SeekError = true;
		if (!BinaryFile::SetPosition(position))
			return false;

		m_FilePos = position;
		m_SeekError = false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Truncate()
{
	if (!m_pBuffer) return false;

	if (m_CachedSize)
		PurgeCache();
	else if (m_BufferPos && !WriteCache())
		return false;

	if (m_SeekError)
	{
		m_SeekError = !BinaryFile::SetPosition(m_FilePos);
		if (m_SeekError) return false;
	}

	return BinaryFile::Truncate();
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Write(const void* pBuffer, size_t bytesToWrite)
{
	if (!m_pBuffer) return false;
	if (!bytesToWrite) return true;
	if (!pBuffer) return false;

	if (m_CachedSize) PurgeCache();
	while (bytesToWrite >= m_BufferSize - m_BufferPos)
	{
		if (m_BufferPos)
		{
			if (!WriteCache()) return false;
			if (bytesToWrite < m_BufferSize)
				break;
		}
		return WriteToFile(pBuffer, bytesToWrite);
	}

#if !AML_DEBUG
	if (bytesToWrite < 16)
	{
		uint8_t* pOut = &m_pBuffer[m_BufferPos];
		const uint8_t* pIn = static_cast<const uint8_t*>(pBuffer);
		for (size_t i = 0; i < bytesToWrite; ++i)
			pOut[i] = pIn[i];
	} else
#endif
	memcpy(&m_pBuffer[m_BufferPos], pBuffer, bytesToWrite);

	m_BufferPos += bytesToWrite;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool BufferedFile::WriteCache()
{
	bool res = WriteToFile(m_pBuffer, m_BufferPos);
	if (res) m_BufferPos = 0;
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool BufferedFile::WriteToFile(const void* pData, size_t bytesToWrite)
{
	if (!m_SeekError || BinaryFile::SetPosition(m_FilePos))
	{
		m_SeekError = true;
		if (BinaryFile::Write(pData, bytesToWrite))
		{
			m_FilePos += bytesToWrite;
			m_SeekError = false;
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   MemoryFile
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
MemoryFile::MemoryFile()
	: m_pFirst(0)
	, m_pBlock(0)
{
}

//----------------------------------------------------------------------------------------------------------------------
MemoryFile::~MemoryFile()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------
MemoryFile& MemoryFile::operator =(MemoryFile&& file)
{
	if (&file != this)
	{
		Close();

		m_pFirst = file.m_pFirst;
		m_pBlock = file.m_pBlock;
		m_BlockPos = file.m_BlockPos;

		m_Size = file.m_Size;
		m_Position = file.m_Position;
		m_OpenFlags = file.m_OpenFlags;

		file.m_pFirst = file.m_pBlock = 0;
		file.m_OpenFlags = 0;
	}
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::ApplyPosition()
{
	const size_t newPosition = m_Position;

	m_pBlock = m_pFirst;
	m_BlockPos = BLOCK_SIZE;
	m_Position = BLOCK_SIZE;

	while (newPosition > m_Position)
	{
		Grow();
		// Изменим значения m_BlockPos, позиции и размера файла, чтобы наш объект файла был консистентным
		// в случае генерации исключения при нехватке памяти при вызове Grow() в следующей итерации цикла.
		m_BlockPos = BLOCK_SIZE;
		m_Position += BLOCK_SIZE;
		if (m_Position > m_Size)
			m_Size = m_Position;
	}
	m_BlockPos = newPosition - (m_Position - BLOCK_SIZE);
	// Установим размер файла равным новой текущей позиции. Далее мы либо допишем что-то в наш файл и
	// обновим тем самым размер, либо (если это вызов из функции Truncate) это и будет новый размер.
	m_Size = m_Position = newPosition;
}

//----------------------------------------------------------------------------------------------------------------------
void MemoryFile::Close()
{
	for (Block* pBlock = m_pFirst; pBlock;)
	{
		Block* pTemp = pBlock;
		pBlock = pBlock->header.pNext;
		delete pTemp;
	}
	m_pFirst = m_pBlock = 0;
	m_OpenFlags = 0;
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::DoRead(void* pBuffer, size_t bytesToRead)
{
	if (m_BlockPos == BLOCK_SIZE)
	{
		m_pBlock = m_pBlock->header.pNext;
		m_BlockPos = 0;
	}

	for (uint8_t* pOut = static_cast<uint8_t*>(pBuffer);;)
	{
		size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead : bytesLeft;
		memcpy(pOut, m_pBlock->data + m_BlockPos, toRead);

		m_BlockPos += toRead;
		m_Position += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead) return;

		pOut += toRead;
		m_pBlock = m_pBlock->header.pNext;
		m_BlockPos = 0;
	}
}

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::DoWrite(const void* pBuffer, size_t bytesToWrite)
{
	if (!m_pBlock) ApplyPosition();
	if (m_BlockPos == BLOCK_SIZE) Grow();

	for (const uint8_t* pIn = static_cast<const uint8_t*>(pBuffer);;)
	{
		size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		size_t toWrite = (bytesToWrite <= bytesLeft) ? bytesToWrite : bytesLeft;
		memcpy(m_pBlock->data + m_BlockPos, pIn, toWrite);

		m_BlockPos += toWrite;
		m_Position += toWrite;
		if (m_Position > m_Size)
			m_Size = m_Position;

		bytesToWrite -= toWrite;
		if (!bytesToWrite) return;

		pIn += toWrite;
		Grow();
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::GetCRC32Custom(uint32_t& crc, long long size)
{
	if (size <= 0) return true;
	if (m_Position >= m_Size) return false;
	// Проверим, что в файле достаточно данных. Здесь мы можем безопасно привести переменную
	// size к типу size_t, так как размер MemoryFile имеет такой же тип, и пользовательское
	// значение длины, переданное в функцию File::GetCRC32, также имеет тип size_t.
	if (static_cast<size_t>(size) > m_Size - m_Position) return false;

	if (m_BlockPos == BLOCK_SIZE)
	{
		m_pBlock = m_pBlock->header.pNext;
		m_BlockPos = 0;
	}

	for (size_t bytesToRead = static_cast<size_t>(size);;)
	{
		size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead: bytesLeft;
		crc = hash::GetCRC32(m_pBlock->data + m_BlockPos, toRead, crc);

		m_BlockPos += toRead;
		m_Position += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead) return true;

		m_pBlock = m_pBlock->header.pNext;
		m_BlockPos = 0;
	}
}

//----------------------------------------------------------------------------------------------------------------------
inline void MemoryFile::Grow()
{
	if (m_pBlock->header.pNext)
		m_pBlock = m_pBlock->header.pNext;
	else
	{
		Block* pNew = new Block;
		pNew->header.pPrev = m_pBlock;
		pNew->header.pNext = 0;
		m_pBlock->header.pNext = pNew;
		m_pBlock = pNew;
	}
	m_BlockPos = 0;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::LoadFrom(File& file, bool clear)
{
	if (!file.IsOpened() || !(file.GetOpenFlags() & FILE_OPEN_READ))
		return false;

	if (!m_OpenFlags) Open();
	size_t originalPos = m_Position;
	bool res = file.SaveTo(*this, clear);
	SetPosition(clear ? 0 : originalPos);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::LoadFrom(const std::wstring& path, bool clear)
{
	BinaryFile src;
	bool res = false;
	if (src.Open(path, FILE_OPEN_READ))
	{
		res = LoadFrom(src, clear);
		src.Close();
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Open(unsigned flags)
{
	if (m_OpenFlags)
		return false;

	m_pBlock = m_pFirst = new Block;
	m_pFirst->header.pPrev = 0;
	m_pFirst->header.pNext = 0;

	m_BlockPos = 0;
	m_Size = m_Position = 0;

	m_OpenFlags = (flags | FILE_OPEN_MEMORY) & FILE_OPENFLAG_MASK;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Open(const std::wstring& path, unsigned flags)
{
	return Open(flags);
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead)
{
	bytesRead = 0;

	if (!m_OpenFlags) return false;
	if (!bytesToRead || (m_Position >= m_Size)) return true;

	if (bytesToRead > m_Size - m_Position)
		bytesToRead = m_Size - m_Position;

	if (bytesToRead <= BLOCK_SIZE - m_BlockPos)
	{
		uint8_t* pIn = m_pBlock->data + m_BlockPos;
#if !AML_DEBUG
		if (bytesToRead < 16)
		{
			uint8_t* pOut = static_cast<uint8_t*>(pBuffer);
			for (size_t i = 0; i < bytesToRead; ++i)
				pOut[i] = pIn[i];
		} else
#endif
		memcpy(pBuffer, pIn, bytesToRead);

		m_BlockPos += bytesToRead;
		m_Position += bytesToRead;
	} else
		DoRead(pBuffer, bytesToRead);

	bytesRead = bytesToRead;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::SaveToCustom(File& file)
{
	size_t bytesLeft = m_Size;
	for (Block* pBlock = m_pFirst; bytesLeft; pBlock = pBlock->header.pNext)
	{
		size_t toCopy = (bytesLeft < BLOCK_SIZE) ? bytesLeft : BLOCK_SIZE;
		if (!file.Write(pBlock->data, toCopy)) return false;
		bytesLeft -= toCopy;
	}
	return !bytesLeft;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::SetPosition(long long position)
{
	if (!m_OpenFlags || (position < 0)) return false;
	if (position > size_t(-1)) return false;

	size_t newPos = static_cast<size_t>(position);
	if (newPos <= (m_Size | ((m_Size & (BLOCK_SIZE - 1)) ? (BLOCK_SIZE - 1) : 0)))
	{
		size_t p = m_Position - m_BlockPos;
		if (!m_pBlock || (newPos <= p / 2))
		{
			m_pBlock = m_pFirst;
			p = 0;
		}
		if (newPos > p)
		{
			p += BLOCK_SIZE - 1;
			for (; newPos - 1 > p; p += BLOCK_SIZE)
				m_pBlock = m_pBlock->header.pNext;
			p -= BLOCK_SIZE - 1;
		} else
		{
			for (; newPos < p; p -= BLOCK_SIZE)
				m_pBlock = m_pBlock->header.pPrev;
		}
		m_BlockPos = newPos - p;
	} else
	{
		m_pBlock = 0;
		m_BlockPos = BLOCK_SIZE;
	}

	m_Position = newPos;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Truncate()
{
	if (!m_OpenFlags) return false;

	if (!m_pBlock) ApplyPosition();
	for (Block* pBlock = m_pBlock->header.pNext; pBlock;)
	{
		Block* pTemp = pBlock;
		pBlock = pBlock->header.pNext;
		delete pTemp;
	}
	m_pBlock->header.pNext = 0;
	m_Size = m_Position;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Write(const void* pBuffer, size_t bytesToWrite)
{
	if (!m_OpenFlags) return false;
	if (!bytesToWrite) return true;

	if (bytesToWrite <= BLOCK_SIZE - m_BlockPos)
	{
		uint8_t* pOut = m_pBlock->data + m_BlockPos;
#if !AML_DEBUG
		if (bytesToWrite < 16)
		{
			const uint8_t* pIn = static_cast<const uint8_t*>(pBuffer);
			for (size_t i = 0; i < bytesToWrite; ++i)
				pOut[i] = pIn[i];
		} else
#endif
		memcpy(pOut, pBuffer, bytesToWrite);

		m_BlockPos += bytesToWrite;
		m_Position += bytesToWrite;
		if (m_Position > m_Size)
			m_Size = m_Position;
	} else
		DoWrite(pBuffer, bytesToWrite);

	return true;
}
