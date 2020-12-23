//⬪MDPN⬪
#include "pch.h"
#include "util.h"

#include "const.h"
#include "number.h"

#include <core/auxutil.h>
#include <core/array.h>
#include <core/exception.h>
#include <core/file.h>
#include <core/strutil.h>
#include <core/util.h>
#include <core/winapi.h>
#include <zlib/zlib.h>

#include <exception>
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция GuardedCall
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
static int CppExceptionGuard(const std::function<int()>& fn, int errorCode)
{
	try
	{
		return fn ? fn() : 0;
	}
	catch (const util::EGeneric& e)
	{
		aux::Printf("\r#12Fatal error: unhandled exception [%s] -> %s\n", e.ClassName(), e.what());
	}
	catch (const std::exception& e)
	{
		aux::Printf("\r#12Fatal error: unhandled c++ exception -> %s\n", e.what());
	}
	catch (...)
	{
		aux::Printc("\r#12Fatal error: unhandled c++ exception\n");
	}
	return errorCode;
}

//----------------------------------------------------------------------------------------------------------------------
int GuardedCall(const std::function<int()>& fn, int errorCode)
{
	__try
	{
		return CppExceptionGuard(fn, errorCode);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		aux::Printf("\r#12Fatal error: unhandled exception %08X \n",
			::GetExceptionCode());
	}
	return errorCode;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции CompressFile и DecompressFile
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool CompressFile(util::File& src, util::File& dst, int level)
{
	constexpr int MAX_LEVEL = 5;
	level = util::Clamp(level, 0, MAX_LEVEL);
	// Не используем степени 8 и 9, потому что в подавляющем большинстве случаев они не приводят к уменьшению
	// размера сжатых данных (или эта разница незначительна), но сильно снижают скорость работы алгоритма
	static const int zipLevels[1 + MAX_LEVEL] = { 0, 1, 4, 5, 6, 7 };

	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	// Инициализируем zlib. В расширенных параметрах выключаем заголовок zlib (что также отключает
	// и вычисление контрольных сумм adler32). Остальные параметры инициализируем значениями по
	// умолчанию, так как они обеспечивают наилучший баланс между скоростью и степенью сжатия
	if (::deflateInit2(&stream, zipLevels[level], Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return false;

	constexpr size_t BUFFER_SIZE = 128 * 1024;
	uint8_t* pSrcBuffer = new uint8_t[BUFFER_SIZE];
	uint8_t* pDstBuffer = new uint8_t[BUFFER_SIZE];

	bool res;
	int flush = Z_NO_FLUSH;
	do {
		res = false;
		size_t bytesRead;
		if (!src.Read(pSrcBuffer, BUFFER_SIZE, bytesRead))
			break;
		stream.next_in = pSrcBuffer;
		stream.avail_in = static_cast<unsigned>(bytesRead);
		if (bytesRead < BUFFER_SIZE)
			flush = Z_FINISH;

		do {
			stream.next_out = pDstBuffer;
			stream.avail_out = BUFFER_SIZE;
			if (::deflate(&stream, flush) == Z_STREAM_ERROR)
				break;
			size_t bytesToWrite = BUFFER_SIZE - stream.avail_out;
			if (!dst.Write(pDstBuffer, bytesToWrite))
				break;
			res = stream.avail_out > 0;
		} while (!res);
	} while (res && flush == Z_NO_FLUSH);

	delete[] pDstBuffer;
	delete[] pSrcBuffer;
	::deflateEnd(&stream);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
bool DecompressFile(util::File& src, util::File& dst)
{
	z_stream stream;
	memset(&stream, 0, sizeof(stream));
	if (::inflateInit2(&stream, -15) != Z_OK)
		return false;

	constexpr size_t BUFFER_SIZE = 128 * 1024;
	uint8_t* pSrcBuffer = new uint8_t[BUFFER_SIZE];
	uint8_t* pDstBuffer = new uint8_t[BUFFER_SIZE];

	int res = Z_OK;
	do {
		size_t bytesRead;
		if (!src.Read(pSrcBuffer, BUFFER_SIZE, bytesRead))
			break;
		stream.next_in = pSrcBuffer;
		stream.avail_in = static_cast<unsigned>(bytesRead);

		do {
			stream.next_out = pDstBuffer;
			stream.avail_out = BUFFER_SIZE;
			res = ::inflate(&stream, Z_NO_FLUSH);
			if (res != Z_OK && res != Z_STREAM_END)
				break;
			size_t bytesToWrite = BUFFER_SIZE - stream.avail_out;
			if (!dst.Write(pDstBuffer, bytesToWrite))
			{
				res = Z_DATA_ERROR;
				break;
			}
		} while (!stream.avail_out && stream.avail_in);
	} while (res == Z_OK);

	delete[] pDstBuffer;
	delete[] pSrcBuffer;
	::inflateEnd(&stream);
	return res == Z_STREAM_END;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Прочие функции
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool IsNumber(const char* pStr)
{
	if (!pStr || *pStr == 0)
		return false;

	for (const char* p = pStr; *p; ++p)
	{
		if (*p < '0' || *p > '9')
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool IsNumber(const char* pStr, size_t count)
{
	if (!pStr || !count)
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		if (pStr[i] < '0' || pStr[i] > '9')
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
float GetRangeProgress(size_t range, uint64_t count)
{
	static float invTotalA[Const::MAX_DIGIT_C + 1];
	static const bool isInitialized = ([]() {
		uint64_t total = 9;
		for (size_t i = 1; i <= Const::MAX_DIGIT_C; ++i)
		{
			invTotalA[i] = 100.f / total;
			total = (i & 1) ? total * 2 - total / 10 : total * 10;
		}
	}(), true);

	return (range <= Const::MAX_DIGIT_C) ?
		count * invTotalA[range] : 0;
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE std::string SeparateWithCommas(const char* pStr, size_t size, char separator)
{
	const size_t oLen = (size > 3) ? size + (size - 1) / 3 : size;
	util::SmartArray<char, 256> buffer(oLen);

	char* pOut = buffer;
	for (size_t p = 0, l = size % 3; p < size;)
	{
		if (p && separator)
			*pOut++ = separator;
		for (size_t i = (p || !l) ? 3 : l; i; --i)
			*pOut++ = pStr[p++];
	}
	return std::string(buffer, oLen);
}

//----------------------------------------------------------------------------------------------------------------------
std::string SeparateWithCommas(uint64_t number, char separator)
{
	char buffer[24];
	int len = sprintf_s(buffer, "%llu", number);
	return SeparateWithCommas(buffer, static_cast<size_t>(len), separator);
}

//----------------------------------------------------------------------------------------------------------------------
std::string SeparateWithCommas(const Number& number, char separator)
{
	const size_t numLen = number.GetLength();
	util::SmartArray<char> buffer(numLen + 1);
	size_t len = number.AsString(buffer, numLen + 1);
	return SeparateWithCommas(buffer, len, separator);
}

//----------------------------------------------------------------------------------------------------------------------
std::string SeparateWithCommas(const std::string& s, char separator)
{
	return SeparateWithCommas(s.c_str(), s.size(), separator);
}

//----------------------------------------------------------------------------------------------------------------------
std::string FormatSpeed(float speed)
{
	int k = -1;
	float n = .001f * speed;
	while (++k < 3 && n >= 99.95f)
		n *= .001f;

	char formatA[] = "%.0f*/s";
	formatA[2] = (n < 9.995f) ? '2' : '1';
	formatA[4] = "KMGT"[k];

	return util::Format(formatA, n);
}

//----------------------------------------------------------------------------------------------------------------------
std::string FormatSize(uint64_t sizeInBytes, bool colored)
{
	int k = -1;
	float n = (1.f / 1024) * sizeInBytes;
	while (++k < 3 && n >= 999.5f)
		n *= (1.f / 1024);

	char formatA[] = "%.0f";
	formatA[2] = (n < 99.95f) ? (n < 9.995f) ? '2' : '1' : '0';

	static const char* suffixA[] = { " KiB", " MiB", " GiB", " TiB" };
	static const char* coloredA[] = { "#8KiB#7", "#8MiB#7", "#8GiB#7", "#8TiB#7" };

	return util::Format(formatA, n) + (colored ? coloredA[k] : suffixA[k]);
}
