//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/platform.h>
#include <core/util.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   ThreadTimer - получение процессорного времени потока
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class ThreadTimer
{
	AML_NONCOPYABLE(ThreadTimer)

public:
	ThreadTimer();
	~ThreadTimer();

	// Сбрасывает счётчик
	void Reset();

	// Возвращает суммарное время работы потока (в микросекундах)
	// с момента последнего сброса или создания объекта класса
	uint64_t GetElapsed(bool reset = false);

protected:
	void* m_ThreadHandle = nullptr;
	uint64_t m_KernelTime = 0;
	uint64_t m_UserTime = 0;
};
