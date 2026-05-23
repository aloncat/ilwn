#pragma once

#include "../../core/platform.h"
#include "../../core/singleton.h"
#include "../../core/util.h"
#include "membarrier.h"
#include "threadsync.h"

#include <new.h>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
#define AML_SIMPLE_SINGLETON(NAME) \
	AML_SIMPLE_SINGLETON_EX(NAME,)

//----------------------------------------------------------------------------------------------------------------------
#define AML_SIMPLE_SINGLETON_EX(NAME, CTOR)	\
	public:										\
		static NAME& Instance();				\
	private:									\
		NAME() CTOR;							\
		AML_NONCOPYABLE(NAME);					\
		static NAME* CreateObj();				\
		static NAME* volatile m_pInstance;

//----------------------------------------------------------------------------------------------------------------------
#define AML_IMPLEMENT_SIMPLE_SINGLETON(NAME)			\
	NAME* volatile NAME::m_pInstance = 0;				\
	NAME& NAME::Instance() {							\
		if (NAME* pObj = m_pInstance) {					\
			thread::ReadMemBarrier();					\
			return *pObj;								\
		}												\
		return *CreateObj();							\
	}													\
	AML_NOINLINE NAME* NAME::CreateObj() {				\
		static volatile char spinLock = 0;				\
		thread::AutoFastSpinLock lock(spinLock);		\
		if (NAME* pObj = m_pInstance) {					\
			thread::ReadMemBarrier();					\
			return pObj;								\
		}												\
		static uint8_t data[sizeof(NAME)];				\
		NAME* pObj = new(data) NAME;					\
		thread::WriteMemBarrier();						\
		m_pInstance = pObj;								\
		return pObj;									\
	}

//----------------------------------------------------------------------------------------------------------------------
#define AML_SINGLETON_EX(NAME, CTOR, DTOR)		\
	public:										\
		static NAME& Instance();				\
	private:									\
		NAME() CTOR;							\
		virtual ~NAME() DTOR;					\
		static NAME* CreateObj();				\
		static SingletonOld* CreateInstance();	\
		static InstancePtr m_pInstance;

//----------------------------------------------------------------------------------------------------------------------
#define AML_IMPLEMENT_SINGLETON(NAME)								\
	util::SingletonOld::InstancePtr NAME::m_pInstance = 0;			\
	NAME& NAME::Instance() {										\
		if (SingletonOld* pObj = m_pInstance) {						\
			thread::ReadMemBarrier();								\
			return *static_cast<NAME*>(pObj);						\
		}															\
		return *CreateObj();										\
	}																\
	util::SingletonOld* NAME::CreateInstance() {					\
		static NAME object;											\
		return &object;												\
	}																\
	AML_NOINLINE NAME* NAME::CreateObj() {							\
		SingletonOld::CreateInstance(m_pInstance, &CreateInstance);	\
		return static_cast<NAME*>(m_pInstance);						\
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SingletonOld
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class SingletonOld
{
	AML_NONCOPYABLE(SingletonOld)

protected:
	using CreateFn = SingletonOld* (*)();
	using InstancePtr = SingletonOld* volatile;

protected:
	SingletonOld();
	virtual ~SingletonOld();

	static void		CreateInstance(InstancePtr& objPtr, CreateFn createFn);

private:
	static void		Init();
	static void		Finalize();

	static void		LogErrorAndAbort(const char* pErrorMsg);
	static bool		PushPopStack(bool doPush, const InstancePtr* pObjPtr);

private:
	InstancePtr*	m_pObjPtr;

	static thread::CriticalSection*	volatile	m_pLock;
	static std::vector<const InstancePtr*>*		m_pObjStack;
	static volatile bool						m_IsFinalizing;
};

} // namespace util
