#pragma once

#include "../../core/platform.h"
#include "../../core/threadsync.h"
#include "../../core/util.h"

#include <type_traits>

namespace thread {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FastSpinLock
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс FastSpinLock реализует простейший спинлок без счетчика захватов/освобождений. После SPIN_COUNT
// итераций цикла ожидания, управление передается системе с помощью функции thread::Thread::Sleep(0).
// В системах с одним логическим процессором значение SPIN_COUNT игнорируется (считается равным 0).

//----------------------------------------------------------------------------------------------------------------------
class FastSpinLock
{
	friend class AutoFastSpinLock;
	AML_NONCOPYABLE(FastSpinLock)

public:
	FastSpinLock() : m_Lock(0) {}

	void			Enter() { Acquire(m_Lock); }
	void			Leave() { Release(m_Lock); }

	static void		Acquire(volatile char& lock);
	static void		Release(volatile char& lock);

private:
	// Количество циклов ожидания освобождения объекта
	// до передачи управления операционной системе.
	static const size_t SPIN_COUNT = 60;

	static void		SpinAndAcquire(volatile char& lock);

	volatile char	m_Lock;
};

//----------------------------------------------------------------------------------------------------------------------
class AutoFastSpinLock
{
	AML_NONCOPYABLE(AutoFastSpinLock)

public:
	explicit AutoFastSpinLock(volatile char* pLock)
		: m_pLock(pLock)
	{
		if (pLock)
			FastSpinLock::Acquire(*pLock);
	}

	explicit AutoFastSpinLock(volatile char& lock)
		: m_pLock(&lock)
	{
		FastSpinLock::Acquire(lock);
	}

	explicit AutoFastSpinLock(FastSpinLock& spinLock)
		: m_pLock(&spinLock.m_Lock)
	{
		FastSpinLock::Acquire(*m_pLock);
	}

	~AutoFastSpinLock()
	{
		if (m_pLock)
			FastSpinLock::Release(*m_pLock);
	}

	void Leave()
	{
		if (m_pLock)
		{
			FastSpinLock::Release(*m_pLock);
			m_pLock = 0;
		}
	}

private:
	volatile char* m_pLock;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SpinLock
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс SpinLock реализует спинлок со счетчиком захватов/освобождений. Опциональный параметр конструктора
// spinCount задает количество итераций цикла ожидания перед передачей управления операционной системе,
// реализуемое через вызов функции thread::Thread::Sleep(0). В системах с одним логическим процессором
// значение spinCount игнорируется (считается равным 0).

//----------------------------------------------------------------------------------------------------------------------
class SpinLock
{
	AML_NONCOPYABLE(SpinLock)

public:
	using Lock = Lock<SpinLock>;

	explicit SpinLock(unsigned spinCount = 100);

	bool TryEnter();

	void Enter();
	void Leave();

private:
	void SpinAndAcquire(uint32_t currentThreadId);

	volatile uint32_t m_OwningThread;	// ID потока, владеющего спинлоком (0, если спинлок свободен)
	volatile unsigned m_LockCounter;	// Счетчик текущего количества активных захватов
	const unsigned m_SpinCount;
};

} // namespace thread
