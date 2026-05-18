#pragma once

#include "../../core/file.h"
#include "../../core/platform.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class BufferedFile : public BinaryFile
{
public:
	// Параметр bufferSize задает размер буфера в килобайтах. Этот буфер будет использован
	// для операций чтения (упреждающее чтение) или операций записи (кеширование записи).
	BufferedFile(size_t bufferSize = 16);
	virtual ~BufferedFile();

	virtual bool		Open(const std::wstring& path, unsigned flags = FILE_OPEN_READ);
	// Закрывает файл. Если во внутреннем буфере есть несохраненные данные, то будет вызвана функция
	// FlushBuffer(). Однако при этом не гарантируется, что данные будут полностью сохранены в файл.
	// При возниковении ошибки записи в файл данные могут быть сохранены частично или могут быть не
	// сохранены вовсе. Чтобы гарантированно сохранить накопленные в буфере данные в файл, нужно
	// перед вызовом Close() вызвать функцию FlushBuffer() и проверить возвращенное ей значение.
	virtual void		Close();

	using				BinaryFile::Read;
	// Читает не более bytesToRead байт из файла в pBuffer. Если во внутреннем буфере
	// есть несохраненные данные, то они будут сохранены в файл до операциии чтения.
	// При этом, если во время записи произойдет ошибка, функция вернет false.
	virtual bool		Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead);
	// Записывает bytesToWrite байт из pBuffer в файл. По возможности (при достаточности
	// внутреннего буфера) данные будут кешированы. Чтобы сохранить кешированные данные
	// в файл, нужно использовать функцию FlushBuffer() или функцию Flush().
	virtual bool		Write(const void* pBuffer, size_t bytesToWrite);

	// Сохраняет накопленное содержимое внутреннего буфера (кеш записи)
	// в файл, а затем сбрасывает системный буфер файла на накопитель.
	virtual bool		Flush();
	// Сохраняет накопленное содержимое внутреннего буфера записи в файл (при
	// этом запись данных в файл может кешироваться операционной системой).
	bool				FlushBuffer();

	virtual long long	GetSize() const;
	virtual long long	GetPosition() const;
	virtual bool		SetPosition(long long position);
	virtual bool		Truncate();

protected:
	void				PurgeCache();
	bool				WriteCache();

	bool				ReadFromFile(void* pBuffer, size_t bytesToRead, size_t& bytesRead);
	bool				WriteToFile(const void* pData, size_t bytesToWrite);

protected:
	uint8_t*			m_pBuffer;		// Указатель на внутренний буфер
	const size_t		m_BufferSize;	// Размер внутреннего буфера в байтах
	size_t				m_BufferPos;	// Позиция чтения/записи внутри буфера
	size_t				m_CachedSize;	// Байт данных в буфере (режим чтения)
	mutable long long	m_FilePos;		// Позиция файла, соответствующая состоянию буфера
	mutable bool		m_SeekError;	// false, если m_FilePos равен реальной позиции файла
};

//----------------------------------------------------------------------------------------------------------------------
class MemoryFile : public File
{
public:
	MemoryFile();
	virtual ~MemoryFile();

	bool				Open(unsigned flags = FILE_OPEN_MEMORY);
	virtual bool		Open(const std::wstring& path, unsigned flags = FILE_OPEN_MEMORY);
	virtual void		Close();

	virtual bool		IsOpened() const { return (m_OpenFlags != 0); }

	using				File::Read;
	virtual bool		Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead);
	virtual bool		Write(const void* pBuffer, size_t bytesToWrite);

	virtual long long	GetSize() const { return m_OpenFlags ? m_Size : -1; }
	virtual long long	GetPosition() const { return m_OpenFlags ? m_Position : -1; }
	virtual bool		SetPosition(long long position);
	virtual bool		Truncate();

	// Загружает содержимое указанного файла. Если параметр clear равен true, то содержимое этого
	// файла удаляется (полностью заменяется содержимым указанного файла), а позиция устанавливается
	// в начало. Если clear равен false, то данные загружаются с текущей позиции и сама позиция не
	// изменяется. Первая функция изменяет позицию загружаемого файла file.
	bool				LoadFrom(File& file, bool clear = true);
	bool				LoadFrom(const std::wstring& path, bool clear = true);

	MemoryFile&			operator =(MemoryFile&& file);

protected:
	static const unsigned	FILE_OPEN_MEMORY = FILE_OPEN_READWRITE | FILE_CREATE_ALWAYS;
	static const size_t		BLOCK_SIZE = 64 * 1024;  // Размер блока (степень 2)

	struct Block;
	struct BlockHeader {
		Block*		pPrev;
		Block*		pNext;
	};
	struct Block {
		BlockHeader	header;
		uint8_t		data[BLOCK_SIZE];
	};

protected:
	virtual bool		SaveToCustom(File& file);
	virtual bool		GetCRC32Custom(uint32_t& crc, long long size);

	void				Grow();
	void				ApplyPosition();
	void				DoRead(void* pBuffer, size_t bytesToRead);
	void				DoWrite(const void* pBuffer, size_t bytesToWrite);

protected:
	Block*			m_pFirst;		// Первый блок списка
	Block*			m_pBlock;		// Текущий блок (соответствующий текущей позиции)
	size_t			m_BlockPos;		// Текущая позиция в текущем блоке

	size_t			m_Size;			// Размер файла в байтах
	size_t			m_Position;		// Текущая позиция файла
};

} // namespace util
