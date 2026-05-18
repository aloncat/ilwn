//⬪AML⬪
#pragma once

#include "platform.h"
#include "util.h"

#include <string>

namespace util {

enum {
	FILE_OPEN_READ		= 0x01,		// Открыть файл с доступом на чтение
	FILE_OPEN_WRITE		= 0x02,		// Открыть файл с доступом на запись
	FILE_OPEN_READWRITE	= 0x03,		// То же, что и FILE_OPEN_READ | FILE_OPEN_WRITE
	FILE_OPEN_ALWAYS	= 0x04,		// Создать файл, если не существует; открыть, если существует
	FILE_CREATE_ALWAYS	= 0x08,		// Создать файл, если не существует; перезаписать, если существует
	FILE_DENY_READ		= 0x10,		// Запретить другим процессам доступ к файлу на чтение

	FILE_OPENFLAG_MASK	= 0x1f
};

//----------------------------------------------------------------------------------------------------------------------
class File
{
	AML_NONCOPYABLE(File)

public:
	File();
	virtual ~File() = default;

	virtual bool Open(const std::wstring& path, unsigned flags) = 0;
	virtual void Close() { m_OpenFlags = 0; }

	virtual bool IsOpened() const = 0;
	unsigned GetOpenFlags() const { return m_OpenFlags; }

	// Читает точно bytesToRead байт в буфер pBuffer. В случае ошибки возвращает false, позиция
	// файла не определена. Если файл короче, то позиция файла устанавливается на конец файла
	// и функция возвращает false, заполняя буфер только прочитанными данными
	bool Read(void* pBuffer, size_t bytesToRead);

	// Читает не более bytesToRead байт. Возвращает количество реально прочитанных байт в bytesRead.
	// Если файл короче, то функция вернёт true, а значение bytesRead будет меньше bytesToRead
	virtual bool Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead) = 0;
	// Записывает bytesToWrite байт из буфера pBuffer в файл
	virtual bool Write(const void* pBuffer, size_t bytesToWrite) = 0;
	// Сбрасывает все данные из кэша (буфера записи) в физический файл
	virtual bool Flush() { return IsOpened(); }

	// Функции получения размера файла и его текущей позиции (только для открытого файла).
	// В случае ошибки (а также если файл не открыт) возвращают отрицательное значение
	virtual long long GetSize() const = 0;
	virtual long long GetPosition() const = 0;
	// Установка текущей позиции файла
	virtual bool SetPosition(long long position) = 0;
	// Усечение размера файла по текущей позиции. Если текущая позиция файла находится
	// за концом данных, файл будет дополнен мусором до необходимого размера
	virtual bool Truncate() = 0;

	// Вычисляет CRC32 файла. Значение возвращается в параметре crc. Параметр position задаёт
	// начало блока данных; значение 0 соответствует началу файла, значение -1 соответствует
	// текущей позиции. Параметр size задаёт размер блока; если параметр не задан (равен -1),
	// то концом блока считается конец файла. Функция изменяет текущую позицию файла
	bool GetCRC32(uint32_t& crc, long long position = 0, size_t size = ~size_t(0));

	// Сохраняет файл целиком в указанный файл file. Если параметр clearDest равен true, то
	// всё содержимое файла назначения file удаляется. Если равен false, то запись начинается
	// с текущей позиции; если file длиннее, чем записываемые данные, то остальная часть файла
	// не изменяется. Функция меняет текущую позицию обоих файлов
	bool SaveTo(File& file, bool clearDest = false);
	// Сохраняет файл целиком в другой файл по указанному пути path. Если такой файл существует,
	// то его содержимое будет полностью заменено. Функция меняет текущую позицию этого файла
	bool SaveTo(const std::wstring& path);

protected:
	virtual bool SaveToCustom(File& file);
	virtual bool GetCRC32Custom(uint32_t& crc, long long size);

	unsigned m_OpenFlags;
};

//----------------------------------------------------------------------------------------------------------------------
class BinaryFile : public File
{
public:
	BinaryFile();
	virtual ~BinaryFile() override;

	virtual bool Open(const std::wstring& path, unsigned flags = FILE_OPEN_READ) override;
	virtual void Close() override;

	virtual bool IsOpened() const override;

	using File::Read;
	virtual bool Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead) override;
	virtual bool Write(const void* pBuffer, size_t bytesToWrite) override;
	virtual bool Flush() override;

	virtual long long GetSize() const override;
	virtual long long GetPosition() const override;
	virtual bool SetPosition(long long position) override;
	virtual bool Truncate() override;

protected:
	struct FileSystem;
	void* m_FileHandle;
};

} // namespace util
