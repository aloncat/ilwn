//⬪AML⬪
#include "pch.h"
#include "file.h"

#include "array.h"
#include "crc32.h"
#include "filesystem.h"
#include "winapi.h"

using namespace util;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   File
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
File::File()
	: m_OpenFlags(0)
{
}

//----------------------------------------------------------------------------------------------------------------------
bool File::Read(void* pBuffer, size_t bytesToRead)
{
	size_t bytesRead;
	bool res = Read(pBuffer, bytesToRead, bytesRead);
	return res && bytesRead == bytesToRead;
}

//----------------------------------------------------------------------------------------------------------------------
bool File::GetCRC32(uint32_t& crc, long long position, size_t size)
{
	crc = 0;
	if (!IsOpened() || !(m_OpenFlags & FILE_OPEN_READ))
		return false;

	long long bytesToProcess = size;
	if (size == -1)
	{
		long long fileSize = GetSize();
		if (fileSize < 0)
			return false;
		long long filePosition = position;
		if (filePosition < 0)
		{
			filePosition = GetPosition();
			if (filePosition < 0)
				return false;
		}
		bytesToProcess = fileSize - filePosition;
		if (bytesToProcess <= 0)
			return true;
	}
	else if (bytesToProcess <= 0)
		return size == 0;

	if (position >= 0 && !SetPosition(position))
		return false;

	return GetCRC32Custom(crc, bytesToProcess);
}

//----------------------------------------------------------------------------------------------------------------------
bool File::SaveTo(File& file, bool clearDest)
{
	if (!IsOpened() || !(m_OpenFlags & FILE_OPEN_READ))
		return false;
	if (!file.IsOpened() || !(file.m_OpenFlags & FILE_OPEN_WRITE))
		return false;

	if (clearDest && !file.SetPosition(0))
		return false;

	bool res = SaveToCustom(file);

	if (clearDest && !file.Truncate())
		return false;

	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool File::SaveTo(const std::wstring& path)
{
	BinaryFile dest;
	bool res = false;
	if (dest.Open(path, FILE_OPEN_WRITE | FILE_CREATE_ALWAYS))
	{
		res = SaveTo(dest, false);
		dest.Close();
	}
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool File::SaveToCustom(File& file)
{
	long long bytesCopied = 0;
	const long long bytesToCopy = GetSize();
	if (bytesToCopy > 0 && SetPosition(0))
	{
		const size_t BLOCK_SIZE = 64 * 1024;
		DynamicArray<uint8_t> buffer(BLOCK_SIZE);

		for (size_t bytesRead; bytesCopied < bytesToCopy; bytesCopied += bytesRead)
		{
			if (!Read(buffer, BLOCK_SIZE, bytesRead) || !bytesRead)
				break;
			if (!file.Write(buffer, bytesRead))
				break;
		}
	}
	return bytesCopied == bytesToCopy;
}

//----------------------------------------------------------------------------------------------------------------------
bool File::GetCRC32Custom(uint32_t& crc, long long size)
{
	const size_t BLOCK_SIZE = 64 * 1024;
	DynamicArray<uint8_t> buffer(BLOCK_SIZE);

	unsigned long long bytesLeft = size;
	for (size_t bytesRead; bytesLeft; bytesLeft -= bytesRead)
	{
		size_t toProcess = (bytesLeft < BLOCK_SIZE) ?
			static_cast<size_t>(bytesLeft) : BLOCK_SIZE;
		if (!Read(buffer, toProcess, bytesRead) || !bytesRead)
			return false;
		crc = hash::GetCRC32(buffer, bytesRead, crc);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BinaryFile::FileSystem
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
struct BinaryFile::FileSystem : public util::FileSystem
{
	#if AML_OS_WINDOWS
	using util::FileSystem::MakeLongPath;
	#endif
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BinaryFile
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !AML_OS_WINDOWS
	// Класс BinaryFile реализован только под платформу Windows
	#error Not implemented
#endif

//----------------------------------------------------------------------------------------------------------------------
BinaryFile::BinaryFile()
	: m_FileHandle(INVALID_HANDLE_VALUE)
{
}

//----------------------------------------------------------------------------------------------------------------------
BinaryFile::~BinaryFile()
{
	Close();
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Open(const std::wstring& path, unsigned flags)
{
	if (IsOpened() || path.empty())
		return false;

	if ((flags & FILE_OPEN_READWRITE) == 0)
		flags |= FILE_OPEN_READ;

	DWORD desiredAccess = 0;
	if (flags & FILE_OPEN_READ)
		desiredAccess |= GENERIC_READ;
	if (flags & FILE_OPEN_WRITE)
		desiredAccess |= GENERIC_WRITE;

	DWORD shareMode = FILE_SHARE_READ;
	if (flags & FILE_DENY_READ)
		shareMode = 0;

	DWORD createDisposition = OPEN_EXISTING;
	if (flags & FILE_CREATE_ALWAYS)
		createDisposition = CREATE_ALWAYS;
	else if (flags & FILE_OPEN_ALWAYS)
		createDisposition = OPEN_ALWAYS;

	std::wstring tmpPath;
	auto pLongPath = FileSystem::MakeLongPath(path, tmpPath);
	m_FileHandle = ::CreateFileW(pLongPath, desiredAccess, shareMode,
		nullptr, createDisposition, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	m_OpenFlags = flags & FILE_OPENFLAG_MASK;
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void BinaryFile::Close()
{
	if (m_FileHandle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_FileHandle);
		m_FileHandle = INVALID_HANDLE_VALUE;
		m_OpenFlags = 0;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::IsOpened() const
{
	return m_FileHandle != INVALID_HANDLE_VALUE;
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Read(void* pBuffer, size_t bytesToRead, size_t& bytesRead)
{
	bytesRead = 0;
	if (m_FileHandle == INVALID_HANDLE_VALUE || !pBuffer)
		return false;

	if (sizeof(bytesToRead) == sizeof(DWORD))
	{
		DWORD bytesActuallyRead = 0;
		DWORD toRead = static_cast<DWORD>(bytesToRead);
		BOOL res = ::ReadFile(m_FileHandle, pBuffer, toRead, &bytesActuallyRead, nullptr);
		bytesRead = bytesActuallyRead;
		return res != 0;
	} else
	{
		const size_t MAX_READ_SIZE = 0x80000000;
		for (uint8_t* pData = reinterpret_cast<uint8_t*>(pBuffer);;)
		{
			DWORD bytesActuallyRead = 0;
			const size_t toRead = (bytesToRead <= MAX_READ_SIZE) ? bytesToRead : MAX_READ_SIZE;
			BOOL res = ::ReadFile(m_FileHandle, pData, static_cast<DWORD>(toRead), &bytesActuallyRead, nullptr);
			bytesRead += bytesActuallyRead;
			if (res == 0)
				return false;

			bytesToRead -= toRead;
			if (!bytesToRead || toRead != bytesActuallyRead)
				return true;
			pData += toRead;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Write(const void* pBuffer, size_t bytesToWrite)
{
	if (m_FileHandle == INVALID_HANDLE_VALUE || !pBuffer)
		return false;

	DWORD bytesWritten;
	if (sizeof(bytesToWrite) == sizeof(DWORD))
	{
		DWORD toWrite = static_cast<DWORD>(bytesToWrite);
		BOOL res = ::WriteFile(m_FileHandle, pBuffer, toWrite, &bytesWritten, nullptr);
		return res != 0;
	} else
	{
		const size_t MAX_WRITE_SIZE = 0x80000000;
		for (const uint8_t* pData = reinterpret_cast<const uint8_t*>(pBuffer);;)
		{
			size_t toWrite = (bytesToWrite <= MAX_WRITE_SIZE) ? bytesToWrite : MAX_WRITE_SIZE;
			if (::WriteFile(m_FileHandle, pData, static_cast<DWORD>(toWrite), &bytesWritten, nullptr) == 0)
				return false;

			bytesToWrite -= toWrite;
			if (!bytesToWrite)
				return true;
			pData += toWrite;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Flush()
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	BOOL res = ::FlushFileBuffers(m_FileHandle);
	return res != 0;
}

//----------------------------------------------------------------------------------------------------------------------
long long BinaryFile::GetSize() const
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return -1;

	ULARGE_INTEGER fileSize;
	fileSize.LowPart = ::GetFileSize(m_FileHandle, &fileSize.HighPart);

	if (fileSize.LowPart == INVALID_FILE_SIZE)
	{
		if (::GetLastError() != NO_ERROR)
			return -1;
	}
	return static_cast<long long>(fileSize.QuadPart);
}

//----------------------------------------------------------------------------------------------------------------------
long long BinaryFile::GetPosition() const
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return -1;

	LARGE_INTEGER filePosition;
	filePosition.HighPart = 0;
	filePosition.LowPart = ::SetFilePointer(m_FileHandle, 0, &filePosition.HighPart, FILE_CURRENT);

	if (filePosition.LowPart == INVALID_SET_FILE_POINTER)
	{
		if (::GetLastError() != NO_ERROR)
			return -1;
	}
	return filePosition.QuadPart;
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::SetPosition(long long position)
{
	if (m_FileHandle == INVALID_HANDLE_VALUE || position < 0)
		return false;

	LARGE_INTEGER filePosition;
	filePosition.QuadPart = position;
	filePosition.LowPart = ::SetFilePointer(m_FileHandle, filePosition.LowPart, &filePosition.HighPart, FILE_BEGIN);

	if (filePosition.LowPart == INVALID_SET_FILE_POINTER)
	{
		if (::GetLastError() != NO_ERROR)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Truncate()
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	BOOL res = ::SetEndOfFile(m_FileHandle);
	return res != 0;
}
