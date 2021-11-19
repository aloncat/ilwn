//∙MDPN
#include "number.h"
#include "util.h"

#include <core/auxutil.h>
#include <core/winapi.h>

#include <cuda.h>

constexpr unsigned MAX_DIGITS = 36;			// Максимальное число знаков в исходном числе
constexpr unsigned MAX_ITERATIONS = 480;	// Максимальное количество выполняемых итераций
constexpr unsigned MAX_RES_DIGITS = 256;	// Макс. длина обрабатываемого числа (результата)

//--------------------------------------------------------------------------------------------------------------------------------
struct NumberInfo
{
	uint32_t digitCount;			// Количество знаков в числе (байт в массиве digits)
	uint32_t iterationCount;		// Количество итераций к выполнению (вход) / реально выполнено (выход)
	uint8_t digits[MAX_DIGITS];		// Цифры исходного числа, в обратном порядке (от младшей цифры к старшей)
	uint32_t resultLength;			// Длина результирующего числа (само число возвращается в ResultInfo)

public:
	std::string ToString() const;
};

//--------------------------------------------------------------------------------------------------------------------------------
std::string NumberInfo::ToString() const
{
	if (!digitCount || digitCount > MAX_DIGITS)
		throw std::exception();

	char buffer[MAX_DIGITS];
	auto last = digitCount - 1;
	for (unsigned i = 0; i <= last; ++i)
		buffer[i] = '0' + digits[last - i];

	return std::string(buffer, digitCount);
}

//--------------------------------------------------------------------------------------------------------------------------------
class MyNumber : public BigNumber
{
public:
	NumberInfo GetInfo() const;
};

//--------------------------------------------------------------------------------------------------------------------------------
NumberInfo MyNumber::GetInfo() const
{
	if (!m_Length || m_Length > 32)
		throw std::exception();

	NumberInfo result;

	result.digitCount = m_Length;
	result.resultLength = m_Length;
	result.iterationCount = MAX_ITERATIONS;

	// NB: обнуляем байты последнего неполного 64-битного слова.
	// Это необходимо для корректного чтения числа в коде ядра CUDA
	*reinterpret_cast<uint64_t*>(result.digits + ((m_Length - 1) & ~7)) = 0;
	memcpy(result.digits, m_DigitA, m_Length);

	return result;
};

//--------------------------------------------------------------------------------------------------------------------------------
struct ResultInfo
{
	uint8_t digits[MAX_RES_DIGITS];

public:
	std::string ToString(const NumberInfo& info) const;
};

//--------------------------------------------------------------------------------------------------------------------------------
std::string ResultInfo::ToString(const NumberInfo& info) const
{
	if (!info.resultLength || info.resultLength > MAX_RES_DIGITS)
		throw std::exception();

	char buffer[MAX_RES_DIGITS];
	auto last = info.resultLength - 1;
	for (unsigned i = 0; i <= last; ++i)
		buffer[i] = '0' + digits[last - i];

	return std::string(buffer, info.resultLength);
}

__constant__ unsigned shuffleMask[4];
__constant__ unsigned lengthMask[4];

//--------------------------------------------------------------------------------------------------------------------------------
__global__ void numKernel(NumberInfo* data, ResultInfo* results)
{
	NumberInfo& info = data[blockIdx.x];			// Задание (исходное число и параметры)
	ResultInfo& result = results[blockIdx.x];		// Результат вычислений (число)

	const unsigned ti = threadIdx.x;				// Индекс потока (64-битного слова)
	unsigned length = info.digitCount;				// Текущее количество цифр в числе
	unsigned iterationCount = info.iterationCount;	// Количество итераций к выполнению

	unsigned loWord = 0;			// Младшие 4 цифры в 64-битном слове
	unsigned hiWord = 0;			// Старшие 4 цифры в 64-битном слове
	bool testPalindrome = false;	// true, если проверяем палиндром

	// NB: последнее неполное 64-битное слово в info.digits
	// должно содержать нулевые байты для отсутствующих цифр
	if (ti < (length + 7) >> 3)
	{
		loWord = reinterpret_cast<const uint32_t*>(info.digits)[ti << 1];
		hiWord = reinterpret_cast<const uint32_t*>(info.digits)[(ti << 1) + 1];
	}

	// NB: значения info.digitCount и info.iterationCount всегда должны быть >0 на входе.
	// Мы уменьшаем и проверяем значение переменной iterationCount в середине тела цикла

	for (++iterationCount;;)
	{
		--iterationCount;
		uint32_t t, lo = 0, hi = 0;

		// Получаем цифры перевёрнутого числа
		if (ti < (length + 7) >> 3)
		{
			if ((length - 1) & 4)
			{
				const unsigned reverseLane = ((length - 1) >> 3) - ti;
				lo = __shfl_sync(0xffffffff, hiWord, reverseLane);
				hi = __shfl_sync(0xffffffff, loWord, reverseLane);
			} else
			{
				const unsigned lastLane = (length - 1) >> 3;
				lo = __shfl_sync(0xffffffff, loWord, lastLane - ti);

				if (ti < lastLane)
				{
					hi = __shfl_sync(0xffffffff, hiWord, lastLane - ti - 1);
				}
			}
		}

		// NB: по причине оптимизации, макс. длина числа ограничена 249 цифрами.
		// У более длинных чисел значение hiR (при ti == 32) будет некорректным
		const unsigned mask = shuffleMask[length & 3];
		t = __shfl_down_sync(0xffffffff, lo, 1);
		lo = __byte_perm(lo, hi, mask);
		hi = __byte_perm(hi, t, mask);

		// Проверяем получение палиндрома и количество итераций
		if (!__ballot_sync(0xffffffff, (loWord - lo) | (hiWord - hi)))
		{
			if (testPalindrome)
				break;
		}
		if (!iterationCount)
		{
			iterationCount = info.iterationCount;
			break;
		}
		testPalindrome = true;

		// Складываем цифры
		loWord += lo + 0xf6f6f6f6;
		hiWord += hi + 0xf6f6f6f6;

		// Обрабатываем переносы
		lo = ~loWord >> 31;
		t = hi = ~(hiWord + lo) >> 31;
		do {
			loWord += __shfl_sync(0xffffffff, t, (ti - 1) & 31);
			lo = ~loWord >> 31;
			t = hi;
			hi = ~(hiWord + lo) >> 31;
			t = hi - t;
		} while (__ballot_sync(0xffffffff, t));
		hiWord += lo;

		// Удаляем маску переносов
		loWord = (loWord & 0x0f0f0f0f) - ((loWord & 0x60606060) >> 4);
		hiWord = (hiWord & 0x0f0f0f0f) - ((hiWord & 0x60606060) >> 4);

		// Проверяем изменение длины числа
		t = ((length & 4) ? hiWord : loWord) & lengthMask[length & 3];
		length += __shfl_sync(0xffffffff, t, length >> 3) ? 1 : 0;
	}

	if (ti == 0)
	{
		// Сохраняем результат
		info.resultLength = length;
		info.iterationCount = info.iterationCount - iterationCount;
	}

	// Сохраняем в глобальную память результирующее число
	reinterpret_cast<uint32_t*>(result.digits)[ti << 1] = loWord;
	reinterpret_cast<uint32_t*>(result.digits)[(ti << 1) + 1] = hiWord;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void ShowGPUStats()
{
	int deviceCount = 0;
	cudaGetDeviceCount(&deviceCount);
    aux::Printf("Found %d CUDA device(s)\n", deviceCount);

	cudaDeviceProp devProp;
    for (int device = 0; device < deviceCount; ++device)
    {
        cudaGetDeviceProperties(&devProp, device);

		aux::Printf("\nDevice %d:\n", device);
		aux::Printf("  Name:                    %s\n", devProp.name);
		aux::Printf("  Total global memory:     %u MiB\n", devProp.totalGlobalMem / (1024 * 1024));
		aux::Printf("  Compute capability:      %d.%d\n", devProp.major, devProp.minor);
		aux::Printf("  Max active warps per SM: %d\n", devProp.maxThreadsPerMultiProcessor / devProp.warpSize);
		aux::Printf("  Shared memory per block: %u KiB\n", devProp.sharedMemPerBlock / 1024);
		aux::Printf("  Registers per block:     %d\n", devProp.regsPerBlock);
		aux::Printf("  Max threads per block:   %d\n", devProp.maxThreadsPerBlock);
		aux::Printf("  Total constant memory:   %u KiB\n", devProp.totalConstMem / 1024);
		aux::Printf("  Warp size:               %d\n", devProp.warpSize);
	}

	int device;
	cudaGetDevice(&device);
	aux::Printf("\nSelected device: %d\n", device);

	int numBlocks;
	const int blockSize = 32;
	cudaGetDeviceProperties(&devProp, device);
	cudaOccupancyMaxActiveBlocksPerMultiprocessor(&numBlocks, numKernel, blockSize, 0);

	const int activeWarps = numBlocks * blockSize / devProp.warpSize;
	const int maxWarps = devProp.maxThreadsPerMultiProcessor / devProp.warpSize;

	if (activeWarps < maxWarps)
	{
		aux::Printf("#12Warning: #7active warps ratio is %d/%d\n", activeWarps, maxWarps);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static bool CheckNumbers(NumberInfo* numbers, ResultInfo* results, unsigned count)
{
	BigNumber num;
	for (unsigned i = 0; i < count; ++i)
	{
		const NumberInfo& info = numbers[i];
		const ResultInfo& result = results[i];

		const auto numStr = info.ToString();
		num = numStr;

		unsigned stepC = 0;
		if (!num.RAATillPalindrome(375, stepC))
			stepC = 0;

		if (info.iterationCount != stepC || num.AsString() != result.ToString(info))
		{
			aux::Printf("\rError at number %s, steps=%u (%u)\n",
				numStr.c_str(), info.iterationCount, stepC);
			return false;
		}
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void TestCudaSearch()
{
	ShowGPUStats();

	constexpr unsigned numbersInChunk = 32 * 1024; // 1536 KiB
	constexpr size_t CHUNK_DATA_SIZE = numbersInChunk * sizeof(NumberInfo);
	constexpr size_t RES_DATA_SIZE = numbersInChunk * sizeof(ResultInfo);

	NumberInfo* currentChunk = new NumberInfo[numbersInChunk];
	NumberInfo* nextChunk = new NumberInfo[numbersInChunk];
	ResultInfo* resultsData = new ResultInfo[numbersInChunk];

	NumberInfo* gpuDataChunk = nullptr;
	cudaMalloc(&gpuDataChunk, CHUNK_DATA_SIZE);

	ResultInfo* gpuResults = nullptr;
	cudaMalloc(&gpuResults, RES_DATA_SIZE);

	cudaEvent_t gpuEventReady;
	cudaEventCreate(&gpuEventReady);

	const unsigned shuffleMaskValues[4] = { 0x123, 0x5670, 0x6701, 0x7012 };
	cudaMemcpyToSymbol(shuffleMask, shuffleMaskValues, sizeof(shuffleMaskValues));

	const unsigned lengthMaskValues[4] = { 0xff, 0xff00, 0xff0000, 0xff000000 };
	cudaMemcpyToSymbol(lengthMask, lengthMaskValues, sizeof(lengthMaskValues));

	uint32_t lastTick = ::GetTickCount();
	const uint32_t startTick = lastTick;

	MyNumber lastNumber;
	bool hasDataReady = false;
	bool hasNewNumbers = false;
	bool isWaitingForGpu = false;

	lastNumber.Set("10000009999999999999999");
	size_t numbersDone = 0;

	for (;;)
	{
		hasNewNumbers = false;
		if (numbersDone < 5000000)
		{
			hasNewNumbers = true;
			// Готовим следующий блок чисел
			for (unsigned i = 0; i < numbersInChunk; ++i)
			{
				++lastNumber;
				lastNumber.SkipRAADups();
				nextChunk[i] = lastNumber.GetInfo();
				nextChunk[i].iterationCount = 375;
			}
		}

		// Забираем готовый блок чисел
		if (isWaitingForGpu)
		{
			// Синхронизируемся с GPU
			cudaEventSynchronize(gpuEventReady);
			isWaitingForGpu = false;

			// Получаем готовый блок чисел из GPU
			//cudaMemcpy(currentChunk, gpuDataChunk, CHUNK_DATA_SIZE, cudaMemcpyDeviceToHost);
			//cudaMemcpy(resultsData, gpuResults, RES_DATA_SIZE, cudaMemcpyDeviceToHost);
			hasDataReady = true;
		}

		if (hasNewNumbers)
		{
			// Отправляем новый блок чисел в GPU
			cudaMemcpy(gpuDataChunk, nextChunk, CHUNK_DATA_SIZE, cudaMemcpyHostToDevice);

			// Запускаем вычисление на GPU
			numKernel<<<numbersInChunk, 32>>>(gpuDataChunk, gpuResults);

			// Событие для синхронизации
			cudaEventRecord(gpuEventReady, 0);
			isWaitingForGpu = true;
		}

		// Обрабатываем готовые данные
		if (hasDataReady)
		{
			hasDataReady = false;

			/*if (!CheckNumbers(currentChunk, resultsData, numbersInChunk))
			{
				if (isWaitingForGpu)
				{
					cudaEventSynchronize(gpuEventReady);
					isWaitingForGpu = false;
				}
				break;
			}*/

			numbersDone += numbersInChunk;

			const uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 500)
			{
				lastTick = tick;
				auto s = currentChunk[numbersInChunk - 1].ToString();
				aux::Printf("\rTesting %s...", SeparateWithCommas(s).c_str());
			}
		}

		if (!isWaitingForGpu)
			break;

		// Меняем местами буферы заданий
		std::swap(currentChunk, nextChunk);
	}

	const float elapsed = 0.001f * (::GetTickCount() - startTick);
	const float speed = 0.000001f * numbersDone / elapsed;
	aux::Printf("\nDone in %.2fs, %.2f M/s", elapsed, speed);

	cudaEventDestroy(gpuEventReady);

	cudaFree(gpuResults);
	cudaFree(gpuDataChunk);

	delete[] resultsData;
	delete[] nextChunk;
	delete[] currentChunk;
}
