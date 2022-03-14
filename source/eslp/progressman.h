//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/threadsync.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ProgressManager - менеджер прогресса поиска
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class ProgressManager final
{
public:
	// Макс. кол-во коэф-в в выводе прогресса
	static constexpr size_t MAX_COEFS = 8;

	ProgressManager();

	void Reset();

	bool GetProgress(unsigned* factors) const;

	void SetProgress(const unsigned* factors, unsigned taskId);
	void SetDone(unsigned taskId, bool oldest);

private:
	// Макс. количество подряд идущих заданий, для
	// которых хранится состояние прогресса поиска
	static constexpr size_t MAX_TASKS = 8;

	struct Item {
		unsigned taskId = 0;			// Номер задания
		unsigned progress[MAX_COEFS];	// Первые коэффициенты
		bool isReady = false;			// true, если есть данные
		bool isDone = false;			// true, если задание завершено

		void Reset(unsigned id = 0);
	};

	Item* GetItem(unsigned taskId);

private:
	mutable thread::CriticalSection m_CS;

	Item m_Items[MAX_TASKS];			// Кольцевой буфер заданий
	unsigned m_Count = 0;				// Количество заданий в m_Items
	unsigned m_Head = 0;				// Индекс старейшего задания (голова списка)

	unsigned m_Progress[MAX_COEFS];		// Текущий прогресс (первые коэффициенты)
	bool m_IsReady = false;				// true, если m_Progress содержит данные
};
