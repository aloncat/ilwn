#include "fileutil.h"

#include "file.h"

#include <algorithm>

using namespace util;

// FileReader и FileWriter используют кодирование значений переменной длины. Для этого каждому
// значению предшествует байт, хранящий информацию о размере числа (или размере длины строки),
// также хранящий в некоторых случаях старшие биты значения:
//		CW 0....... + 0 б,  7-битное число			CW 110..... + 0 б, 5 бит длины (0 - 31)
//		CW 10...... + 1 б, 14-битное число			CW 111100.. + 1 б, 10 бит длины (32 - 1055)
//		CW 1110.... + 2 б, 20-битное число			CW 111101.. + 2 б, 18 бит длины (1056 - 263199)
//		CW 111110.. + 3 б, 26-битное число			CW 11111100 + 3 б, 24 бита длины (0 - 2^24-1)
//		CW 11111101 + 4 б, 32-битное число
//		CW 11111110 + 6 б, 48-битное число
//		CW 11111111 + 8 б, 64-битное число

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileReader
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
FileReader::FileReader(File& file, unsigned bufferSize)
	: m_File(file)
	, m_BufferSize(1024 * Clamp(bufferSize, 1u, 64u))
	, m_BufferPos(0)
	, m_DataSize(0)
{
	m_pBuffer = new uint8_t[m_BufferSize];
}

//----------------------------------------------------------------------------------------------------------------------
FileReader::~FileReader()
{
	Purge();
	AML_SAFE_DELETEA(m_pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::FillBuffer()
{
	m_DataSize -= m_BufferPos;
	memcpy(m_pBuffer, &m_pBuffer[m_BufferPos], m_DataSize);
	m_BufferPos = 0;

	size_t bytesRead;
	if (m_File.Read(&m_pBuffer[m_DataSize], m_BufferSize - m_DataSize, bytesRead))
	{
		m_DataSize += static_cast<unsigned>(bytesRead);
		return (bytesRead > 0);
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::Purge()
{
	unsigned bytesToBack = m_DataSize - m_BufferPos;
	if (bytesToBack == 0) return true;

	long long position = m_File.GetPosition();
	if ((position < 0) || (position < bytesToBack)) return false;
	if (!m_File.SetPosition(position - bytesToBack)) return false;
	m_DataSize = m_BufferPos;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
#define READ_L_S32 \
	static const unsigned se[4][2] = {{ 0, ~0x0000007fu }, { 0, ~0x00003fffu },				\
									  { 0, ~0x000fffffu }, { 0, ~0x03ffffffu }};			\
	if (v < 0xc0) {																			\
		if (v <= 0x7f) {																	\
			value = v | se[0][(v >> 6) & 1]; ++m_BufferPos;									\
			return true; }																	\
								   v = ((v & 0x3f) <<  8) | se[1][(v >> 5) & 1]; l = 1; }	\
	else if ((v & 0xf0) == 0xe0) { v = ((v & 0x0f) << 16) | se[2][(v >> 3) & 1]; l = 2; }	\
	else if ((v & 0xfc) == 0xf8) { v = ((v & 0x03) << 24) | se[3][(v >> 1) & 1]; l = 3; }	\
	else if  (v			== 0xfd) { v = 0; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define READ_L_U32 \
	if (v < 0xc0) {													\
		if (v <= 0x7f) {											\
			value = v; ++m_BufferPos;								\
			return true; }											\
								   v = (v & 0x3f) <<  8; l = 1; }	\
	else if ((v & 0xf0) == 0xe0) { v = (v & 0x0f) << 16; l = 2; }	\
	else if ((v & 0xfc) == 0xf8) { v = (v & 0x03) << 24; l = 3; }	\
	else if  (v			== 0xfd) { v = 0; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define READ_L_S64 \
	static const unsigned se[4][2] = {{ 0, ~0x0007fu }, { 0, ~0x0003fffu },					\
									  { 0, ~0xfffffu }, { 0, ~0x3ffffffu }};				\
	if (v < 0xc0) {																			\
		if (v <= 0x7f) {																	\
			value = (int32_t)(v | se[0][(v >> 6) & 1]); ++m_BufferPos;						\
			return true; }																	\
								   v = ((v & 0x3f) <<  8) | se[1][(v >> 5) & 1]; l = 1; }	\
	else if  (v > 0xfc)			 { l = 2 + 2 * (v & 3); v = 0; }							\
	else if ((v & 0xf0) == 0xe0) { v = ((v & 0x0f) << 16) | se[2][(v >> 3) & 1]; l = 2; }	\
	else if ((v & 0xfc) == 0xf8) { v = ((v & 0x03) << 24) | se[3][(v >> 1) & 1]; l = 3; }

//----------------------------------------------------------------------------------------------------------------------
#define READ_L_U64 \
	if (v < 0xc0) {													\
		if (v <= 0x7f) {											\
			value = v; ++m_BufferPos;								\
			return true; }											\
								   v = (v & 0x3f) <<  8; l = 1; }	\
	else if  (v > 0xfc)			 { l = 2 + 2 * (v & 3);  v = 0;	}	\
	else if ((v & 0xf0) == 0xe0) { v = (v & 0x0f) << 16; l = 2; }	\
	else if ((v & 0xfc) == 0xf8) { v = (v & 0x03) << 24; l = 3; }

//----------------------------------------------------------------------------------------------------------------------
#define PREREAD_V \
	if (m_BufferPos + l >= m_DataSize)						\
	{														\
		if (!FillBuffer()) return false;					\
		if (m_BufferPos + l >= m_DataSize) return false;	\
	}														\
	uint8_t* pBuf = &m_pBuffer[m_BufferPos];				\
	m_BufferPos += 1 + l;

//----------------------------------------------------------------------------------------------------------------------
#define READ_V_32 \
	PREREAD_V;									\
	for (;;)									\
	{											\
		v |= pBuf[1];		if (l == 1) break;	\
		v |= pBuf[2] <<  8; if (l == 2) break;	\
		v |= pBuf[3] << 16; if (l == 3) break;	\
		v |= pBuf[4] << 24; break;				\
	}											\
	value = v;

//----------------------------------------------------------------------------------------------------------------------
#define READ_V_S64 \
	PREREAD_V;											\
	if (l <= 4)											\
	{													\
		for (;;)										\
		{												\
			v |= pBuf[1];		if (l == 1) break;		\
			v |= pBuf[2] <<  8; if (l == 2) break;		\
			v |= pBuf[3] << 16; if (l == 3) break;		\
			v |= pBuf[4] << 24; break;					\
		}												\
		value = (int32_t) v;							\
	} else												\
	{													\
		v = (pBuf[1] | (pBuf[2] << 8)) |				\
			(pBuf[3] << 16) | (pBuf[4] << 24);			\
		unsigned hi = pBuf[5] | (pBuf[6] << 8);			\
		if (l == 8)										\
			hi |= (pBuf[7] << 16) | (pBuf[8] << 24);	\
		else if (hi & 0x8000)							\
			hi |= ~0xffff;								\
		value = ((unsigned long long) hi << 32) | v;	\
	}

//----------------------------------------------------------------------------------------------------------------------
#define READ_V_U64 \
	PREREAD_V;													\
	if (l <= 4)													\
	{															\
		for (;;)												\
		{														\
			v |= pBuf[1];		if (l == 1) break;				\
			v |= pBuf[2] <<  8; if (l == 2) break;				\
			v |= pBuf[3] << 16; if (l == 3) break;				\
			v |= pBuf[4] << 24; break;							\
		}														\
		value = v;												\
	} else														\
	{															\
		v = (pBuf[1] | (pBuf[2] << 8)) |						\
			(pBuf[3] << 16) | (pBuf[4] << 24);					\
		unsigned hi = pBuf[5] | (pBuf[6] << 8);					\
		if (l == 8) hi |= (pBuf[7] << 16) | (pBuf[8] << 24);	\
		value = ((unsigned long long) hi << 32) | v;			\
	}

//----------------------------------------------------------------------------------------------------------------------
#define RETURN_ERROR(SIZE) { \
	size = SIZE;	\
	return false;	\
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadBool(bool& value)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) return false;

	uint8_t v = m_pBuffer[m_BufferPos];
	if (v > 0x7f) return false;

	++m_BufferPos;
	value = (v != 0);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadInt(int& value)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) return false;

	unsigned l, v = m_pBuffer[m_BufferPos];
	READ_L_S32 else return false;

	READ_V_32;
	if ((sizeof(int) > 4) && (value & (1 << 31)))
		value |= -1 << 31;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadInt(long long& value)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) return false;

	unsigned l, v = m_pBuffer[m_BufferPos];
	READ_L_S64 else return false;

	READ_V_S64;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadInt(unsigned& value)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) return false;

	unsigned l, v = m_pBuffer[m_BufferPos];
	READ_L_U32 else return false;

	READ_V_32;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadInt(unsigned long long& value)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) return false;

	unsigned l, v = m_pBuffer[m_BufferPos];
	READ_L_U64 else return false;

	READ_V_U64;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadString(std::string& value)
{
	char buffer[3840];
	// Пытаемся прочитать строку в буфер в стеке. Если строка
	// небольшая, нам не придется выделять память в куче.
	size_t size = sizeof(buffer);
	if (ReadString(buffer, size))
	{
		value.assign(buffer, size);
		return true;
	}
	// Функция вернула false. Если при этом size равен 0, значит
	// проблема не в буфере и произошла ошибка чтения из файла.
	if (size == 0) return false;

	// Выделим достаточный буфер и попробуем снова.
	char* pBuffer = new char[size];
	bool res = ReadString(pBuffer, size);
	if (res) value.assign(pBuffer, size);
	delete[] pBuffer;
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileReader::ReadString(void* pBuffer, size_t& size)
{
	if (m_BufferPos == m_DataSize)
		if (!FillBuffer()) RETURN_ERROR(0);

	// Читаем размер строки.
	size_t bufferSize = size;
	unsigned l, v = m_pBuffer[m_BufferPos];
	if ((v & 0xe0) == 0xc0)
	{
		size = v & 0x1f;
		if (!pBuffer || (size >= bufferSize))
			RETURN_ERROR(size + 1);
		++m_BufferPos;
		if (size == 0)
		{
			*(uint8_t*) pBuffer = 0;
			return true;
		}
	} else
	{
		if		((v & 0xfc) == 0xf0) { size = 32   + ((v & 0x03) <<  8); l = 1; }
		else if ((v & 0xfc) == 0xf4) { size = 1056 + ((v & 0x03) << 16); l = 2; }
		else if  (v			== 0xfc) { size = 0; l = 3; }
		else RETURN_ERROR(0);

		if (m_BufferPos + l >= m_DataSize)
		{
			if (!FillBuffer()) RETURN_ERROR(0);
			if (m_BufferPos + l >= m_DataSize) RETURN_ERROR(0);
		}
		uint8_t* pBuf = &m_pBuffer[m_BufferPos];
		size += pBuf[1]; if (l > 1)	{
			size += pBuf[2] << 8; if (l > 2) {
				size += pBuf[3] << 16;
				if (size == 0) return false; }}
		if (!pBuffer || (size >= bufferSize))
			RETURN_ERROR(size + 1);
		m_BufferPos += 1 + l;
	}

	// Читаем содержимое строки в буфер.
	uint8_t* pOut = (uint8_t*) pBuffer;
	unsigned bytesToCopy = (unsigned) size;
	for (;;)
	{
		if (m_BufferPos < m_DataSize)
		{
			unsigned bytesCopied = std::min(bytesToCopy, m_DataSize - m_BufferPos);
			uint8_t* pIn = &m_pBuffer[m_BufferPos];
			if (bytesCopied < 16)
			{
				unsigned i = 0;
				do pOut[i] = pIn[i]; while (++i < bytesCopied);
			}
			else
				memcpy(pOut, pIn, bytesCopied);
			m_BufferPos += bytesCopied;
			bytesToCopy -= bytesCopied;
			if (bytesToCopy == 0)
			{
				pOut[bytesCopied] = 0;
				return true;
			}
			pOut += bytesCopied;
		}
		if (bytesToCopy >= m_BufferSize) break;
		if (!FillBuffer()) RETURN_ERROR(0);
	}
	size_t bytesRead;
	if (!m_File.Read(pOut, bytesToCopy, bytesRead) || (bytesRead != bytesToCopy)) RETURN_ERROR(0);
	pOut[bytesRead] = 0;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileWriter
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
FileWriter::FileWriter(File& file, unsigned bufferSize)
	: m_File(file)
	, m_BufferSize(1024 * Clamp(bufferSize, 1u, 64u))
	, m_BufferPos(0)
{
	m_pBuffer = new uint8_t[m_BufferSize];
}

//----------------------------------------------------------------------------------------------------------------------
FileWriter::~FileWriter()
{
	Flush();
	AML_SAFE_DELETEA(m_pBuffer);
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::Flush()
{
	if (m_BufferPos > 0)
	{
		if (!m_File.Write(m_pBuffer, m_BufferPos))
			return false;
		m_BufferPos = 0;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_S32N \
	if (v > ~0x2000) {															\
		if (v > ~0x40) {														\
			*pOut = v & 0x7f; ++m_BufferPos;									\
			return true; }														\
							   *pOut = (uint8_t)(0xbf & (v >>  8)); l = 1; }	\
	else if (v > ~0x0080000) { *pOut = (uint8_t)(0xef & (v >> 16)); l = 2; }	\
	else if (v > ~0x2000000) { *pOut = (uint8_t)(0xfb & (v >> 24)); l = 3; }	\
	else					 { *pOut = 0xfd; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_S32P \
	if (v <= 0x1fff) {															\
		if (v <= 0x3f) {														\
			*pOut = (uint8_t) v; ++m_BufferPos;									\
			return true; }														\
							   *pOut = (uint8_t)(0x80 | (v >>  8)); l = 1; }	\
	else if (v <= 0x007ffff) { *pOut = (uint8_t)(0xe0 | (v >> 16)); l = 2; }	\
	else if (v <= 0x1ffffff) { *pOut = (uint8_t)(0xf8 | (v >> 24)); l = 3; }	\
	else					 { *pOut = 0xfd; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_U32 \
	if (v <= 0x3fff) {															\
		if (v <= 0x7f) {														\
			*pOut = (uint8_t) v; ++m_BufferPos;									\
			return true; }														\
							   *pOut = (uint8_t)(0x80 | (v >>  8)); l = 1; }	\
	else if (v <= 0x00fffff) { *pOut = (uint8_t)(0xe0 | (v >> 16)); l = 2; }	\
	else if (v <= 0x3ffffff) { *pOut = (uint8_t)(0xf8 | (v >> 24)); l = 3; }	\
	else					 { *pOut = 0xfd; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_S64N \
	if (v > ~0x8000) { *pOut = 0xfe; l = 2; } \
	else			 { *pOut = 0xff; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_S64P \
	if (v <= 0x7fff) { *pOut = 0xfe; l = 2; } \
	else			 { *pOut = 0xff; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define GET_L_U64 \
	if (v <= 0xffff) { *pOut = 0xfe; l = 2; } \
	else			 { *pOut = 0xff; l = 4; }

//----------------------------------------------------------------------------------------------------------------------
#define WRITE_V \
	m_BufferPos += 1 + l;								\
	*(++pOut) = (uint8_t) v;							\
	while (--l) { v >>= 8; *(++pOut) = (uint8_t) v; }	\
	return true;

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteBool(bool value)
{
	if (m_BufferPos == m_BufferSize)
		if (!Flush()) return false;

	m_pBuffer[m_BufferPos++] = value ? 1 : 0;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteInt(int value)
{
	if (m_BufferPos + 5 > m_BufferSize)
		if (!Flush()) return false;

	uint8_t* pOut = &m_pBuffer[m_BufferPos];
	uint32_t l, v = (uint32_t) value;

	if (value >= 0) GET_L_S32P else GET_L_S32N;
	WRITE_V;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteInt(long long value)
{
	if (m_BufferPos + 9 > m_BufferSize)
		if (!Flush()) return false;

	uint8_t* pOut = &m_pBuffer[m_BufferPos];
	uint32_t l, v = (uint32_t) value;

	unsigned long long uv = (unsigned long long) value;
	if ((unsigned)((uv + (1u << 31)) >> 32) == 0)
		if ((int32_t) v >= 0) GET_L_S32P else GET_L_S32N
	else
	{
		pOut[1] = (uint8_t) v; v >>= 8; pOut[2] = (uint8_t) v; v >>= 8;
		pOut[3] = (uint8_t) v; v >>= 8; pOut[4] = (uint8_t) v;

		v = uv >> 32;
		if ((int32_t) v >= 0) GET_L_S64P else GET_L_S64N;
		m_BufferPos += 4; pOut += 4;
	}
	WRITE_V;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteInt(unsigned value)
{
	if (m_BufferPos + 5 > m_BufferSize)
		if (!Flush()) return false;

	uint8_t* pOut = &m_pBuffer[m_BufferPos];
	uint32_t l, v = (uint32_t) value;

	GET_L_U32;
	WRITE_V;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteInt(unsigned long long value)
{
	if (m_BufferPos + 9 > m_BufferSize)
		if (!Flush()) return false;

	uint8_t* pOut = &m_pBuffer[m_BufferPos];
	uint32_t l, v = (uint32_t) value;

	uint32_t vh = value >> 32;
	if (vh == 0)
		GET_L_U32
	else
	{
		pOut[1] = (uint8_t) v; v >>= 8; pOut[2] = (uint8_t) v; v >>= 8;
		pOut[3] = (uint8_t) v; v >>= 8; pOut[4] = (uint8_t) v;

		v = vh;
		GET_L_U64;
		m_BufferPos += 4; pOut += 4;
	}
	WRITE_V;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileWriter::WriteString(const void* pData, size_t size)
{
	if (!pData) return false;
	if (m_BufferPos + 4 > m_BufferSize)
		if (!Flush()) return false;

	// Выводим длину строки.
	uint8_t* pOut = &m_pBuffer[m_BufferPos];
	if (size <= 31)
	{
		++m_BufferPos;
		*pOut = 0xc0 | (uint8_t) size;
		if (size == 0) return true;
	} else
	{
		unsigned l, v = (unsigned) size;

		if		(size <=	 1055) { v -=	32; *pOut = (uint8_t)(0xf0 | (v >>  8)); l = 1; }
		else if (size <=   263199) { v -= 1056; *pOut = (uint8_t)(0xf4 | (v >> 16)); l = 2; }
		else if (size <= 0xffffff) { *pOut = 0xfc; l = 3; }
		else return false;

		m_BufferPos += 1 + l;
		*(++pOut) = (uint8_t) v;
		while (--l) { v >>= 8; *(++pOut) = (uint8_t) v; }
	}
	++pOut;

	// А теперь сама строка.
	const uint8_t* pIn = (uint8_t*) pData;
	unsigned len = (unsigned) size;
	if (m_BufferPos + len > m_BufferSize)
	{
		if (!Flush()) return false;
		if (len >= m_BufferSize)
			return m_File.Write(pIn, len);
		pOut = m_pBuffer;
	}
	m_BufferPos += len;
	if (len < 16)
	{
		unsigned i = 0;
		do pOut[i] = pIn[i]; while (++i < len);
	} else
		memcpy(pOut, pIn, len);
	return true;
}
