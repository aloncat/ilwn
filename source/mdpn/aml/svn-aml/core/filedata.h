#pragma once

#include "../../core/platform.h"
#include "../../core/util.h"
#include "forward.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class FileData
{
	AML_NONCOPYABLE(FileData)

public:
	FileData();
	virtual ~FileData() {}

	// Загружает данные из указанного файла.
	virtual bool	Load(const std::wstring& filePath);
	// Сохраняет данные в файл. Если forceSave равен false и данные не изменялись (m_IsChanged == false),
	// то функция вернет true, ничего не делая. Если filePath не указан (пустая строка), то данные будут
	// сохранены в тот же файл, из которого были загружены (в который сохранялись в последний раз). Если
	// forceSave равен true, то данные будут сохранены в файл независимо от состояния флага m_IsChanged.
	virtual bool	Save(bool forceSave = false, const std::wstring& filePath = L"");

	bool			IsChanged() const { return m_IsChanged; }

protected:
	static const unsigned FORCE_COMPRESSION = 0x100;	// Флаг: всегда сжимать данные
	static const unsigned MAX_COMPRESSION_LEVEL	= 5;	// Максимальная степень сжатия (младшие 4 бита)

	struct FileHeader {
		uint32_t	magic;		// Магическое число и версия формата
		uint32_t	fileSize;	// Полный размер всего файла в байтах
		uint32_t	headerCRC;	// CRC32 полей magic + dataSize + dataCRC
		uint32_t	dataSize;	// Размер распакованных данных (или 0, если данные не сжаты)
		uint32_t	dataCRC;	// CRC32 распакованных (несжатых) данных

		FileHeader() : magic(0), fileSize(0), headerCRC(0), dataSize(0), dataCRC(0) {}

		int			GetMagic() const { return AML_TO_LE32(magic) & 0xffffff; }
		int			GetVersion() const { return AML_TO_LE32(magic) >> 24; }
	};

protected:
	virtual int		GetHeaderMagic() const { return 'A' | ('M' << 8) | ('L' << 16); }
	virtual int		GetHeaderVersion() const { return 1; }

	virtual void	Clear() {}

	virtual bool	LoadCustom(File& file, int version) = 0;
	virtual bool	SaveCustom(File& file) = 0;

	// Загружает данные из файла m_FilePath, распаковывает их, проверяет CRC и
	// помещает в data, помещает в version версию формата из заголовка файла.
	bool			LoadData(MemoryFile& data, int& version);
	// Сжимает данные из data, добавляет к ним заголовок,
	// вычисляет CRC и сохраняет все в файл m_FilePath.
	bool			SaveData(MemoryFile& data);

	bool			CompressFile(File& src, File& dst);
	bool			DecompressFile(File& src, File& dst);
	bool			CompressData(File& data, MemoryFile& compressed);
	bool			DecompressData(File& data, MemoryFile& decompressed);

protected:
	std::wstring	m_FilePath;				// Путь к файлу, из которого были загружены данные
	unsigned		m_CompressionLevel;		// Степень сжатия данных: 0 = без сжатия (по умолчанию 3)
	bool			m_IsChanged;			// true, если есть несохраненные изменения в данных
};

} // namespace util
