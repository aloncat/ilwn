#pragma once

#include "../../core/platform.h"
#include "../../core/util.h"
#include "forward.h"
#include "strutil.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class FileReader
{
	AML_NONCOPYABLE(FileReader)

public:
	FileReader(File& file, unsigned bufferSize = 8);
	~FileReader();

	bool	ReadBool(bool& value);

	bool	ReadInt(int& value);
	bool	ReadInt(long long& value);
	bool	ReadInt(unsigned& value);
	bool	ReadInt(unsigned long long& value);

	bool	ReadString(std::string& value);
	// Читает строку в буфер pBuffer. Параметр size задает размер буфера в байтах. Если размер буфера
	// достаточен, то функция запишет в него строку и терминирующий 0, сохранит в size размер строки
	// (без учета терминирующего ноля) и вернет true. Если pBuffer равен 0 или размер буфера слишком
	// мал, то функция вернет false, а size будет содержать необходимый размер буфера. В случае
	// ошибки чтения функция вернет false, а size будет равен 0.
	bool	ReadString(void* pBuffer, size_t& size);

	// Удаляет остатки данных во внутреннем буфере и корректирует позицию файла. Во время работы текущая
	// позиция файла будет дальше - там, где закончились данные, прочитанные в буфер. Если, например, мы
	// закончили чтение, но в буфере еще остались данные, вызов этой функции возвращает текущую позицию
	// файла на то место, где мы только что закончили читать данные. Функция возвращает false, если не
	// удалось изменить позицию файла.
	bool	Purge();

protected:
	bool	FillBuffer();

protected:
	File&			m_File;
	uint8_t*		m_pBuffer;
	const unsigned	m_BufferSize;
	unsigned		m_BufferPos;
	unsigned		m_DataSize;
};

//----------------------------------------------------------------------------------------------------------------------
class FileWriter
{
	AML_NONCOPYABLE(FileWriter)

public:
	FileWriter(File& file, unsigned bufferSize = 8);
	~FileWriter();

	bool	WriteBool(bool value);

	bool	WriteInt(int value);
	bool	WriteInt(long long value);
	bool	WriteInt(unsigned value);
	bool	WriteInt(unsigned long long value);

	bool	WriteString(const char* pStr) { return WriteString(pStr, pStr ? Strlen(pStr) : 0); }
	bool	WriteString(const std::string& value) { return WriteString(value.c_str(), value.size()); }
	// Записывает строку (блок данных), на которую указывает pData.
	// Параметр size задает длину строки (блока данных) в байтах.
	bool	WriteString(const void* pData, size_t size);

	// Записывает в файл все данные, накопленные во внутреннем буфере. Функция
	// возвращает false, если не удалось сохранить данные в файл.
	bool	Flush();

protected:
	File&			m_File;
	uint8_t*		m_pBuffer;
	const unsigned	m_BufferSize;
	unsigned		m_BufferPos;
};

} // namespace util
