//⬪AML⬪
#include "pch.h"
#include "crc32.h"

#include "util.h"

#include <atomic>

namespace hash {

static const uint32_t CRC32_POLINOMIAL = 0xedb88320; // Полином CRC32
static const size_t BLOCK_SIZE = 1 << 8; // Размер блока в байтах для параллельного вычисления CRC (2^N)

static uint32_t crc32Table[4][256]; // Таблица предвычисленных CRC для всех байтовых значений
static uint32_t crc32Zeros[4][256]; // Таблица для сдвига вычисленной CRC блока на BLOCK_SIZE нолей

static volatile bool crc32TablesReady = false;

//----------------------------------------------------------------------------------------------------------------------
#if AML_LITTLE_ENDIAN
	#define CRC_TABLE(T, N) T[3 - (N)]
	#define CRC_NEXT_T crc32Table[3][t & 0xff] ^ (t >> 8)
	#define CRC_DO_1BYTE(CRC) CRC = crc32Table[3][(*p++ ^ CRC) & 0xff] ^ (CRC >> 8)
#elif AML_BIG_ENDIAN
	#define CRC_TABLE(T, N) T[N]
	#define CRC_NEXT_T crc32Table[0][t >> 24] ^ (t << 8)
	#define CRC_DO_1BYTE(CRC) CRC = crc32Table[0][*p++ ^ (CRC >> 24)] ^ ((CRC & 0xffffff) << 8)
#else
	#error Unrecognized architecture
#endif

//----------------------------------------------------------------------------------------------------------------------
static void FillCRC32Table()
{
	for (unsigned i = 0; i < 256; ++i)
	{
		uint32_t mask, t = i;
		for (int j = 0; j < 8; ++j)
		{
			mask = 0 - (t & 1);
			t = (t >> 1) ^ (mask & CRC32_POLINOMIAL);
		}
		CRC_TABLE(crc32Table, 0)[i] = AML_TO_LE32(t);
	}
	for (unsigned i = 0; i < 256; ++i)
	{
		uint32_t t = CRC_TABLE(crc32Table, 0)[i];
		for (int j = 1; j < 4; ++j)
		{
			t = CRC_NEXT_T;
			CRC_TABLE(crc32Table, j)[i] = t;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
static inline uint32_t GF2VectorByMatrix(uint32_t vector, const uint32_t* pMatrix)
{
	if (vector == 0)
		return 0;

	// Умножаем вектор-строку vector на матрицу pMatrix над конечным полем 2 элементов. Каждый элемент строки
	// матрицы и вектора - это один бит. Матрица pMatrix должна иметь как минимум столько строк (элементов
	// массива), сколько бит содержится в vector, считая слева на право от самого старшего ненулевого бита
	while ((vector & 0x0f) == 0)
	{
		pMatrix += 4;
		vector >>= 4;
	}
	uint32_t res = 0;
	do {
		res ^= (0 - (vector & 1)) & *pMatrix++;
		vector >>= 1;
	} while (vector);
	return res;
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE void GF2MatrixSquare(uint32_t* pProduct, const uint32_t* pMatrix)
{
	// Умножаем квадратную матрицу 32x32 pMatrix на саму себя над конечным полем GF(2).
	// Результат сохраняем в pProduct. Обе матрицы должны иметь не менее 32 строк
	for (int n = 0; n < 32; ++n)
	{
		pProduct[n] = GF2VectorByMatrix(pMatrix[n], pMatrix);
	}
}

//----------------------------------------------------------------------------------------------------------------------
static void BuildZerosOp(uint32_t* opA, size_t len)
{
	// Строим оператор opA для длины len. Для корректной работы длина len должна быть степенью двойки

	uint32_t oddA[32];		// Оператор для нечётных степеней
	uint32_t* evenA = opA;	// Оператор для чётных степеней

	// Поместим в oddA оператор для 1 нулевого бита (первая строка -
	// это наш полином, в остальных строках - единицы по диагонали)
	uint32_t row = 1;
	oddA[0] = CRC32_POLINOMIAL;
	for (int n = 1; n < 32; ++n)
	{
		oddA[n] = row;
		row <<= 1;
	}

	// Поместим в evenA оператор для 2 нулевых бит
	GF2MatrixSquare(evenA, oddA);
	// То же самое в oddA для 4 нулевых бит
	GF2MatrixSquare(oddA, evenA);

	// Продолжим процесс: первое умножение даст в evenA длину в 8 бит, т.е. ровно 1 байт, следующее
	// умножение даст длину 2 байта в oddA и так далее. Каждый раз длина увеличивается вдвое, а len
	// сдвигается вправо, пока не станет равен 0. Так как len является степенью двойки, то один
	// из операторов будет точно соответствовать необходимому размеру в байтах
	do {
		GF2MatrixSquare(evenA, oddA);
		if ((len >>= 1) == 0)
			return;
		GF2MatrixSquare(oddA, evenA);
		len >>= 1;
	} while (len);

	// Цикл закончился и наш оператор находится в oddA,
	// поэтому нам необходимо скопировать его в opA
	for (int n = 0; n < 32; ++n)
		opA[n] = oddA[n];
}

//----------------------------------------------------------------------------------------------------------------------
static void FillCRC32Zeros()
{
	// Проверим флаг готовности на случай, если другой поток уже завершил инициализацию данных таблиц
	if (crc32TablesReady)
		return;

	uint32_t op[32];
	BuildZerosOp(op, BLOCK_SIZE);

	// Строим 4 таблицы предвычисленных значений для сдвига CRC на BLOCK_SIZE
	// нолей. Каждая из таблиц задаёт по 8 бит из 32-битного значения CRC32
	for (uint32_t n = 0; n < 256; ++n)
	{
		CRC_TABLE(crc32Zeros, 3)[n] = AML_TO_LE32(GF2VectorByMatrix(n, op));
		CRC_TABLE(crc32Zeros, 2)[n] = AML_TO_LE32(GF2VectorByMatrix(n << 8, op));
		CRC_TABLE(crc32Zeros, 1)[n] = AML_TO_LE32(GF2VectorByMatrix(n << 16, op));
		CRC_TABLE(crc32Zeros, 0)[n] = AML_TO_LE32(GF2VectorByMatrix(n << 24, op));
	}
}

//----------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE void InitCRC32()
{
	FillCRC32Table();
	FillCRC32Zeros();

	// Перед установкой флага готовности мы должны быть уверены в том, что все данные таблиц сохранены
	// в памяти. Иначе на платформах со слабой моделью памяти существует вероятность того, что другой
	// поток, проверив установленный флаг, сможет использовать частично проинициализированные данные
	std::atomic_thread_fence(std::memory_order_release);
	crc32TablesReady = true;
}

//----------------------------------------------------------------------------------------------------------------------
#define CRC_DO_4BYTES(CRC, P) \
	v = CRC ^ *reinterpret_cast<const uint32_t*>(P); \
	CRC  = crc32Table[0][v & 0xff]; v >>= 8; \
	CRC ^= crc32Table[1][v & 0xff]; v >>= 8; \
	CRC ^= crc32Table[2][v & 0xff] \
		^  crc32Table[3][v >> 8];

//----------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint32_t GetCRC32(const void* pData, size_t size, uint32_t prevHash)
{
	if (!crc32TablesReady)
		InitCRC32();
	// Аналогично инициализации, здесь мы должны быть уверены, что
	// данные таблиц не будут прочитаны раньше, чем флаг готовности
	std::atomic_thread_fence(std::memory_order_acquire);

	size_t crc = AML_TO_LE32(~prevHash);
	// Выравниваем указатель на границу 4 байт
	const uint8_t* p = static_cast<const uint8_t*>(pData);
	for (; size && (reinterpret_cast<size_t>(p) & 3); --size)
		CRC_DO_1BYTE(crc);

	// При достаточном размере данных вычисляем CRC параллельно сразу для двух блоков
	while (size >= 2 * BLOCK_SIZE)
	{
		size_t crc2 = 0;
		for (size_t v, count = BLOCK_SIZE >> 2; count; p += 4, --count)
		{
			CRC_DO_4BYTES(crc, p);
			CRC_DO_4BYTES(crc2, p + BLOCK_SIZE);
		}
		// Сдвигаем CRC первого блока на BLOCK_SIZE нолей, чтобы сложить результат с CRC второго блока
		crc = crc2 ^ crc32Zeros[0][crc & 0xff] ^ crc32Zeros[1][(crc >> 8) & 0xff] ^
			crc32Zeros[2][(crc >> 16) & 0xff] ^ crc32Zeros[3][crc >> 24];
		// Уменьшаем size на размер двух обработанных блоков и приращаем p на размер второго блока
		size -= 2 * BLOCK_SIZE;
		p += BLOCK_SIZE;
	}

	// Вычисляем CRC для всех оставшихся 32-битных слов
	for (size_t v, count = size >> 2; count; p += 4, --count)
	{
		CRC_DO_4BYTES(crc, p);
	}
	// Вычисляем CRC для оставшихся байтов
	for (size_t count = size & 3; count; --count)
		CRC_DO_1BYTE(crc);

	return AML_TO_LE32(static_cast<uint32_t>(~crc));
}

} // namespace hash
