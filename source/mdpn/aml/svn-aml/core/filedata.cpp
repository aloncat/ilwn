#include "filedata.h"

#include "../../core/crc32.h"
#include "file.h"
#include "filesystem.h"

#include "zlib/zlib.h"

#include <algorithm>

using namespace util;

//----------------------------------------------------------------------------------------------------------------------
FileData::FileData()
	: m_CompressionLevel(3)
	, m_IsChanged(false)
{
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::CompressData(File& data, MemoryFile& compressed)
{
	const long long dataSize = data.GetSize();
	bool force = (m_CompressionLevel & FORCE_COMPRESSION) != 0;
	// Если при получении размера файла произошла ошибка или файл пуст, то сжимать
	// нечего. Также нет смысла сжимать данные, если файл короче 512 байт.
	if ((dataSize <= 0) || (!force && (dataSize < 512))) return false;

	if (compressed.Open())
	{
		if (CompressFile(data, compressed))
		{
			if (force) return true;
			long long compressedSize = compressed.GetSize();
			float ratio = static_cast<float>(compressedSize) / dataSize;
			// Сохраним данные в сжатом виде при степени сжатия >= 5%.
			if ((ratio > 0) && (ratio <= .95f)) return true;
		}
		compressed.Close();
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::CompressFile(File& src, File& dst)
{
	// В качестве максимальной степени сжатия zlib используем значение 7. Значения 8
	// и 9 незначительно повышают степень сжатия, но сильно снижают скорость работы.
	static const int levels[1 + MAX_COMPRESSION_LEVEL] = { 0, 1, 4, 5, 6, 7 };
	unsigned level = std::min(m_CompressionLevel & 0xf, MAX_COMPRESSION_LEVEL);
	if ((level == 0) && (m_CompressionLevel & FORCE_COMPRESSION)) level = 1;

	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	// Инициализируем zlib. В расширенных параметрах выключаем заголовок zlib (что также
	// отключает вычисление контрольных сумм adler32). Остальные параметры по умолчанию,
	// так как они обеспечивают наилучший баланс между скоростью и степенью сжатия.
	if (::deflateInit2(&stream, levels[level], Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return false;

	const unsigned BUFFER_SIZE = 128 * 1024;
	uint8_t* pSrcBuffer = new uint8_t[BUFFER_SIZE];
	uint8_t* pDstBuffer = new uint8_t[BUFFER_SIZE];

	bool res = true;
	for (int flush = Z_NO_FLUSH; res && (flush == Z_NO_FLUSH);)
	{
		res = false;
		size_t bytesRead;
		stream.next_in = pSrcBuffer;
		if (!src.Read(pSrcBuffer, BUFFER_SIZE, bytesRead)) break;
		stream.avail_in = static_cast<unsigned>(bytesRead);
		if (bytesRead < BUFFER_SIZE) flush = Z_FINISH;

		do {
			stream.next_out = pDstBuffer;
			stream.avail_out = BUFFER_SIZE;
			if (::deflate(&stream, flush) == Z_STREAM_ERROR) break;
			size_t bytesToWrite = BUFFER_SIZE - stream.avail_out;
			if (!dst.Write(pDstBuffer, bytesToWrite)) break;
			res = (stream.avail_out > 0);
		} while (!res);
	}

	delete[] pDstBuffer;
	delete[] pSrcBuffer;
	::deflateEnd(&stream);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::DecompressData(File& data, MemoryFile& decompressed)
{
	if (decompressed.Open())
	{
		if (DecompressFile(data, decompressed)) return true;
		decompressed.Close();
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::DecompressFile(File& src, File& dst)
{
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	if (::inflateInit2(&stream, -15) != Z_OK)
		return false;

	const unsigned BUFFER_SIZE = 128 * 1024;
	uint8_t* pSrcBuffer = new uint8_t[BUFFER_SIZE];
	uint8_t* pDstBuffer = new uint8_t[BUFFER_SIZE];

	int res = Z_OK;
	do {
		size_t bytesRead;
		stream.next_in = pSrcBuffer;
		if (!src.Read(pSrcBuffer, BUFFER_SIZE, bytesRead) || (bytesRead == 0)) break;
		stream.avail_in = static_cast<unsigned>(bytesRead);

		do {
			stream.next_out = pDstBuffer;
			stream.avail_out = BUFFER_SIZE;
			res = ::inflate(&stream, Z_NO_FLUSH);
			if ((res != Z_OK) && (res != Z_STREAM_END)) break;
			size_t bytesToWrite = BUFFER_SIZE - stream.avail_out;
			if (!dst.Write(pDstBuffer, bytesToWrite)) { res = Z_DATA_ERROR; break; }
		} while (stream.avail_out == 0);
	} while (res == Z_OK);

	delete[] pDstBuffer;
	delete[] pSrcBuffer;
	::inflateEnd(&stream);
	return (res == Z_STREAM_END);
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::Load(const std::wstring& filePath)
{
	Clear();
	m_IsChanged = false;

	m_FilePath = FileSystem::GetFullPath(filePath);
	if (m_FilePath.empty()) return false;

	int version;
	MemoryFile data;
	if (LoadData(data, version) && LoadCustom(data, version))
	{
		// Убеждаемся, что мы прочитали все до последнего байта.
		if (data.GetPosition() == data.GetSize())
			return true;
	}
	Clear();
	m_IsChanged = false;
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::LoadData(MemoryFile& data, int& version)
{
	if (!data.LoadFrom(m_FilePath))
		return false;

	FileHeader header;
	if (!data.Read(&header, sizeof(header)))
		return false;

	if ((header.GetMagic() != GetHeaderMagic()) || (header.GetVersion() > GetHeaderVersion())) return false;
	if (AML_TO_LE32(header.fileSize) != data.GetSize()) return false;
	version = header.GetVersion();

	uint32_t crc = hash::GetCRC32(&header.magic, sizeof(header.magic));
	crc = hash::GetCRC32(&header.dataSize, sizeof(header) - offsetof(FileHeader, dataSize), crc);
	if (AML_TO_LE32(header.headerCRC) != crc) return false;

	uint32_t dataCRC = 0;
	if (header.dataSize)
	{
		MemoryFile decompressed;
		if (!DecompressData(data, decompressed)) return false;
		if (AML_TO_LE32(header.dataSize) != decompressed.GetSize()) return false;
		if (!decompressed.GetCRC32(dataCRC) || (AML_TO_LE32(header.dataCRC) != dataCRC)) return false;
		if (!decompressed.SetPosition(0)) return false;
		data = std::move(decompressed);
	} else
	{
		if (!data.GetCRC32(dataCRC, sizeof(header))) return false;
		if (AML_TO_LE32(header.dataCRC) != dataCRC) return false;
		if (!data.SetPosition(sizeof(header))) return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::Save(bool forceSave, const std::wstring& filePath)
{
	if (!filePath.empty())
	{
		std::wstring newFilePath = FileSystem::GetFullPath(filePath);
		if (!newFilePath.empty()) m_FilePath = std::move(newFilePath);
	}

	if (!m_IsChanged && !forceSave) return true;
	if (m_FilePath.empty()) return false;

	MemoryFile data;
	if (data.Open() && SaveCustom(data) && SaveData(data))
	{
		m_IsChanged = false;
		return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileData::SaveData(MemoryFile& data)
{
	FileHeader header;
	header.magic = AML_TO_LE32(GetHeaderMagic() | (GetHeaderVersion() << 24));

	if (data.GetSize() > uint32_t(-1)) return false;
	header.dataSize = static_cast<uint32_t>(data.GetSize());
	if (!data.GetCRC32(header.dataCRC)) return false;
	header.dataCRC = AML_TO_LE32(header.dataCRC);
	if (!data.SetPosition(0)) return false;

	MemoryFile compressed;
	if (CompressData(data, compressed))
	{
		data.Close();
		long long compressedSize = compressed.GetSize();
		if ((compressedSize <= 0) || (compressedSize > uint32_t(-1) - sizeof(header))) return false;
		header.fileSize = AML_TO_LE32(sizeof(header) + static_cast<uint32_t>(compressedSize));
		header.dataSize = AML_TO_LE32(header.dataSize);
	} else
	{
		if (m_CompressionLevel & FORCE_COMPRESSION) return false;
		header.fileSize = AML_TO_LE32(sizeof(header) + header.dataSize);
		header.dataSize = 0;
	}

	size_t size = sizeof(header) - offsetof(FileHeader, dataSize);
	uint32_t crc = hash::GetCRC32(&header.magic, sizeof(header.magic));
	header.headerCRC = AML_TO_LE32(hash::GetCRC32(&header.dataSize, size, crc));

	BinaryFile file;
	bool res = false;
	if (file.Open(m_FilePath, FILE_OPEN_WRITE | FILE_CREATE_ALWAYS))
	{
		File& toWrite = header.dataSize ? compressed : data;
		res = file.Write(&header, sizeof(header)) && toWrite.SaveTo(file, false);
		file.Close();
	}
	return res;
}
