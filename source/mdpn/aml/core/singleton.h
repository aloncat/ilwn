//⬪AML⬪
#pragma once

#include "platform.h"
#include "threadsync.h"
#include "toggle.h"
#include "util.h"

#include <atomic>
#include <stdlib.h>
#include <vector>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class SingletonHolder
{
	AML_NONCOPYABLE(SingletonHolder)

public:
	// Уничтожает все "destroyable" синглтоны в порядке, обратном порядку, в котором они
	// создавались, вызывая по очереди все добавленные в список функторы в порядке обратном
	// порядку их добавления (функцией SetKiller) и очищая список. При штатном завершении
	// программы функция вызывается автоматически в порядке обработки списка atexit
	static void KillAll();

protected:
	using KillerFn = void(*)();

	SingletonHolder() = default;

	// Инициализирует при первом вызове критическую секцию s_pLock
	static void Initialize();

	// Эта функция должна вызываться при завершении работы (только во время обработки списка atexit)
	static void Finalize();

	// Добавляет функтор fn в последовательность "уничтожения". Первый вызов
	// этой функции добавляет вызов KillAll в глобальный список atexit
	static void SetKiller(KillerFn fn = nullptr);

	// Проверяет, что указатель pObjPtr отсутствует в стеке, создаёт объект стека s_pObjStack
	// (при необходимости) и помещает указатель на его вершину. Если такой указатель уже есть
	// в стеке, то будет вызвана функция LogErrorAndAbort с сообщением об ошибке
	static bool PushObject(void* pObjPtr);

	// Извлекает указатель с вершины стека. Если при этом стек
	// становится пустым, то уничтожает и сам объект стека
	static void PopObject();

	// Выбрасывает исключение EAssertion с сообщением pErrorMsg
	[[noreturn]] static void LogErrorAndAbort(const char* pErrorMsg);

private:
	struct Item {
		KillerFn fn;
		Item* pNext;
	};

protected:
	static thread::CriticalSection* s_pLock;
	static volatile bool s_IsFinalizing;

private:
	static Item* s_pItem;
	static std::vector<void*>* s_pObjStack;
};

//----------------------------------------------------------------------------------------------------------------------
#define AML_SINGLETON(NAME) \
	std::atomic<NAME*> util::Singleton<NAME>::s_pThis;

//----------------------------------------------------------------------------------------------------------------------
template<class T, bool destroyable = false>
class Singleton : private SingletonHolder
{
public:
	static T& Instance()
	{
		if (T* p = s_pThis.load(std::memory_order_relaxed))
			return *p;
		return *CreateInstance();
	}

	static bool InstanceExists() { return s_pThis.load(std::memory_order_relaxed) != nullptr; }

protected:
	Singleton() = default;
	virtual ~Singleton() = default;

	// Эта функция будет вызвана непосредственно перед уничтожением синглтона, т.е. перед вызовом
	// его деструктора. Произойдёт это либо при штатном завершении работы программы, либо при
	// вызове функции SingletonHolder::KillAll (только для синглтонов с destroyable == true)
	virtual void OnDestroy() {}

	// Эта функция должна создавать объект синглтона и возвращать указатель на него. Указатель
	// затем будет использован для уничтожения синглтона вызовом delete. Класс-наследник может
	// не иметь этой функции, если синглтон создаётся конструктором по умолчанию
	static T* Create() { return new CreateHelper; }

private:
	class CreateHelper : public T {
	public:
		using T::Create;
	};

	static AML_NOINLINE T* CreateInstance()
	{
		Initialize();
		thread::CriticalSection::Lock lock(s_pLock);

		if (T* p = s_pThis.load(std::memory_order_relaxed))
			return p;

		// Если мы находимся в процессе завершения работы приложения (флаг s_IsFinalizing установлен, и сейчас
		// вызываются обработчики списка atexit), то инициализация синглтона невозможна, так же как невозможно
		// и корректное продолжение обработки списка atexit. Данная ситуация возникает тогда, когда деструктор
		// какого-либо класса пытается обратиться к функции Instance синглтона, который уже был уничтожен (или
		// не был проинициализирован). Для решения проблемы нужно в конструктор этого класса добавить тот же
		// вызов функции Instance, что и в деструкторе. Это приведёт к тому, что синглтон будет уничтожаться
		// позже класса (т.е. будет всё еще доступен из его деструктора)
		if (s_IsFinalizing)
			LogErrorAndAbort("Attempt to initialize singleton during finalization");

		// Проверим, что синглтон, который мы собираемся проинициализировать, отсутствует в стеке инициализируемых
		// синглтонов. Если он там всё же есть, значит имеет место рекурсия (т.е. либо 2 синглтона обращаются друг
		// к другу из своих конструкторов, либо наш синглтон обращается сам к себе, например при инициализации
		// своего члена данных, который в своём конструкторе обращается к Instance нашего синглтона)
		FuncToggle toggle([](bool doPush) {
			return doPush ? PushObject(&s_pThis) : (PopObject(), false);
		});

		T* pInstance = CreateHelper::Create();

		if (destroyable)
			SetKiller(Destroy);
		else
			atexit(OnExit);

		s_pThis.store(pInstance, std::memory_order_release);
		return pInstance;
	}

	static void Destroy()
	{
		if (Singleton* pThis = s_pThis.load(std::memory_order_relaxed))
		{
			pThis->OnDestroy();
			delete pThis;

			s_pThis.store(nullptr, std::memory_order_release);
		}
	}

	static void OnExit()
	{
		Finalize();
		Destroy();
	}

protected:
	static std::atomic<T*> s_pThis;
};

} // namespace util
