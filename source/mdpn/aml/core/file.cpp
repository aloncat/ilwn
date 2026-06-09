//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "file.h"

using namespace util;

//--------------------------------------------------------------------------------------------------------------------------------
BufferedFile::BufferedFile(size_t bufferSize)
	: m_Buffer(nullptr)
	, m_BufferSize(Clamp(bufferSize, 4u, 512u) * 1024)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
BufferedFile::~BufferedFile()
{
	Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
void BufferedFile::Close()
{
	if (m_Buffer)
	{
		FlushBuffer();
		free(m_Buffer);
		m_Buffer = nullptr;
	}
	BinaryFile::Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Flush()
{
	return FlushBuffer() && BinaryFile::Flush();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::FlushBuffer()
{
	if (!m_Buffer)
		return false;

	// Позиция m_BufferPos, отличная от 0, может означать, что в буфере есть записанные данные.
	// Но если при этом m_CachedSize не равен 0, то значит буфер находится в режиме чтения
	return !m_BufferPos || m_CachedSize || WriteCache();
}

//--------------------------------------------------------------------------------------------------------------------------------
long long BufferedFile::GetPosition() const
{
	if (!m_Buffer)
		return -1;
	// Если в буфере есть прочитанные данные, то независимо от реальной позиции файла
	// (не важно, чему равен m_SeekError), возвращаем текущую позицию чтения в буфере
	if (m_CachedSize)
		return m_FilePos - m_CachedSize + m_BufferPos;
	// Если не было ошибки позиционирования (т.е. m_FilePos совпадает с реальной позицией файла) или
	// в буфере есть записанные данные, то возвращаем известную нам позицию. В случае наличия ошибки
	// позиционирования и данных в буфере записи следующей операцией с файлом станет сохранение
	// данных из буфера, при котором известная позиция станет реальной позицией файла
	if (!m_SeekError || m_BufferPos)
		return m_FilePos + m_BufferPos;

	// Если же была ошибка и данных в буфере нет, попытаемся выяснить реальную
	// текущую позицию файла. В случае успеха запомним ее и сбросим ошибку
	long long pos = BinaryFile::GetPosition();
	if (pos >= 0)
	{
		m_FilePos = pos;
		m_SeekError = false;
	}
	return pos;
}

//--------------------------------------------------------------------------------------------------------------------------------
long long BufferedFile::GetSize() const
{
	long long size = BinaryFile::GetSize();
	// Если в буфере есть записанные данные, то они могут увеличить размер файла,
	// если находятся за концом имеющихся на текущий момент данных в файле
	if (m_BufferPos && !m_CachedSize && (size >= 0))
	{
		long long position = m_FilePos + m_BufferPos;
		if (position >= size)
			return position;
	}
	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Open(const std::wstring& path, unsigned flags)
{
	if (BinaryFile::Open(path, flags))
	{
		m_BufferPos = 0;
		m_CachedSize = 0;
		m_FilePos = 0;
		m_SeekError = false;

		m_Buffer = static_cast<uint8_t*>(malloc(m_BufferSize));
		if (m_Buffer) return true;

		BinaryFile::Close();
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
inline void BufferedFile::PurgeCache()
{
	size_t bytesLeft = m_CachedSize - m_BufferPos;
	m_BufferPos = m_CachedSize = 0;

	m_FilePos -= bytesLeft;
	m_SeekError |= (bytesLeft != 0);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Read(void* buffer, size_t bytesToRead, size_t& bytesRead)
{
	bytesRead = 0;

	if (!m_Buffer)
		return false;
	if (!bytesToRead)
		return true;
	if (!buffer)
		return false;

	if (!m_CachedSize && m_BufferPos && !WriteCache())
		return false;

	uint8_t* out = static_cast<uint8_t*>(buffer);
	for (bool secondPass = false;; secondPass = true)
	{
		size_t bytesLeft = m_CachedSize - m_BufferPos;
		size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead : bytesLeft;

		const uint8_t* in = &m_Buffer[m_BufferPos];
#if !AML_DEBUG
		if (toRead < 16)
		{
			for (size_t i = 0; i < toRead; ++i)
				out[i] = in[i];
		} else
		{
			memcpy(out, in, toRead);
		}
#else
		if (toRead)
			memcpy(out, in, toRead);
#endif
		m_BufferPos += toRead;
		bytesRead += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead || secondPass)
			return true;

		out += toRead;
		m_BufferPos = m_CachedSize = 0;

		size_t bytesActuallyRead = 0;
		if (bytesToRead >= m_BufferSize)
		{
			bool res = ReadFromFile(out, bytesToRead, bytesActuallyRead);
			if (res)
				bytesRead += bytesActuallyRead;
			return res;
		}
		if (!ReadFromFile(m_Buffer, m_BufferSize, bytesActuallyRead))
			return false;
		m_CachedSize = bytesActuallyRead;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool BufferedFile::ReadFromFile(void* buffer, size_t bytesToRead, size_t& bytesRead)
{
	if (!m_SeekError || BinaryFile::SetPosition(m_FilePos))
	{
		m_SeekError = true;
		if (auto rr = BinaryFile::Read(buffer, bytesToRead); rr.second)
		{
			bytesRead = rr.first;
			m_FilePos += rr.first;
			m_SeekError = false;
			return true;
		}
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::SetPosition(long long position)
{
	if (!m_Buffer || (position < 0))
		return false;
	if (m_BufferPos && !m_CachedSize && !WriteCache())
		return false;

	// Если в буфере есть прочитанные данные и новая позиция остается в пределах буфера,
	// то мы можем изменить только m_BufferPos, не меняя реальную текущую позицию файла
	if (m_CachedSize && (position < m_FilePos) && (position >= static_cast<long long>(m_FilePos - m_CachedSize)))
	{
		m_BufferPos = static_cast<size_t>(position - m_FilePos + m_CachedSize);
	} else
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

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Truncate()
{
	if (!m_Buffer)
		return false;

	if (m_CachedSize)
	{
		PurgeCache();
	}
	else if (m_BufferPos && !WriteCache())
	{
		return false;
	}

	if (m_SeekError)
	{
		m_SeekError = !BinaryFile::SetPosition(m_FilePos);
		if (m_SeekError)
			return false;
	}

	return BinaryFile::Truncate();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::Write(const void* buffer, size_t bytesToWrite)
{
	if (!m_Buffer)
		return false;
	if (!bytesToWrite)
		return true;
	if (!buffer)
		return false;

	if (m_CachedSize)
		PurgeCache();

	while (bytesToWrite >= m_BufferSize - m_BufferPos)
	{
		if (m_BufferPos)
		{
			if (!WriteCache())
				return false;
			if (bytesToWrite < m_BufferSize)
				break;
		}
		return WriteToFile(buffer, bytesToWrite);
	}

#if !AML_DEBUG
	if (bytesToWrite < 16)
	{
		uint8_t* out = &m_Buffer[m_BufferPos];
		const uint8_t* in = static_cast<const uint8_t*>(buffer);
		for (size_t i = 0; i < bytesToWrite; ++i)
			out[i] = in[i];
	} else 
#endif
	{
		memcpy(&m_Buffer[m_BufferPos], buffer, bytesToWrite);
	}

	m_BufferPos += bytesToWrite;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BufferedFile::WriteCache()
{
	bool res = WriteToFile(m_Buffer, m_BufferPos);
	if (res)
		m_BufferPos = 0;
	return res;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool BufferedFile::WriteToFile(const void* data, size_t bytesToWrite)
{
	if (!m_SeekError || BinaryFile::SetPosition(m_FilePos))
	{
		m_SeekError = true;
		if (BinaryFile::Write(data, bytesToWrite))
		{
			m_FilePos += bytesToWrite;
			m_SeekError = false;
			return true;
		}
	}
	return false;
}
