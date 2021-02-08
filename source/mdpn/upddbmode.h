//⬪MDPN⬪
#pragma once

#include "assert.h"
#include "const.h"
#include "dbchunk.h"
#include "dbmode.h"
#include "number.h"
#include "numset.h"

#include <core/platform.h>

#include <vector>

class ThreadTime;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   UpdateDBMode - обновление файлов БД (режим работы программы)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class UpdateDBMode final : public DBMode, AssertHelper<>
{
public:
	UpdateDBMode();

	virtual bool Run() override;

private:
	// Каждый элемент массива - количество проверенных чисел в соответствующем диапазоне
	using RangeProgress = uint64_t[Const::MAX_DIGIT_C + 1];

	struct KnownInfo {
		const DBChunkData::DataItems& numbers;	// Сохранённые отложенные палиндромы
		unsigned minSavedStep = 0;				// Шаг, от которого палиндромы сохранялись
		unsigned searchDepth = 0;				// Глубина поиска, с которой проверялись числа
		float CPUTimeShare = 0;					// Кол-во затраченного времени CPU (в ms) из расчёта на 1 число

		KnownInfo(const DBChunkData::DataItems& items) : numbers(items) {}
	};

	struct Progress {
		uint64_t counter = 0;		// Количество проверенных чисел
		unsigned totalSeconds = 0;	// Общее время работы (сек., накопленное значение)
		uint32_t startTime = 0;		// Тик времени в момент начала работы (периодически обновляется)
		uint32_t lastTick = 0;		// Тик, в котором прогресс выводился на экран в последний раз
		float lastSpeed = 0;		// Последнее вычисленное значение скорости проверки чисел
	};

	bool UpdateDataBase();
	bool RemoveOverlaps();

	float UpdateAllChunks(size_t totalChunkC);
	float UpdateChunks(const std::vector<DBChunk*>& chunks);

	void DoSearch(const Number& target, KnownInfo known);

	bool RemoveChunk(DBChunk* pChunk);
	void CreateNewChunk(const Number& first);
	bool ConcatChunks(DBChunk* pPrev, DBChunk* pLast);
	bool LoadChunkData(DBChunk* pChunk, DBChunkState dataState);
	void SaveActiveChunk(const Number& last, ThreadTime& threadTime);
	void SaveActiveChunk();

	bool NeedsUpdate(const DBChunk* pChunk) const;
	unsigned GetMinSavedStep(const DBChunk* pChunk) const;

	bool PrintProgress(uint32_t tick, const Number& last);
	bool CheckIfCancelled();

	NumberSet m_LychThreads;
	DBChunk* m_pActiveChunk = nullptr;	// Текущий (активный) блок
	DBChunk* m_pPrevChunk = nullptr;	// Блок, расположенный перед текущим

	Progress m_Progress;				// Параметры для отслеживания прогресса проверки чисел
	RangeProgress m_RangeProgressA;		// Количество проверенных чисел в каждом из диапазонов
	size_t m_SavedFileC = 0;			// Общее количество сохранённых файлов

	Number m_Last;						// Последнее проверенное число
	unsigned m_CPUTime = 0;				// Затраченное на проверку время CPU (в ms)

	bool m_IsExecuted = false;			// true, если функция Run была вызвана
	bool m_IsCancelled = false;			// true, если пользователь отменил операцию
	bool m_DontFillGaps = false;		// true, если не нужно проверять пропущенные интервалы
	bool m_From1stKnown = false;		// true, если нужно начать с первого проверенного числа в БД
};
