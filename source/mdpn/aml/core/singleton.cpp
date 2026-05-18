//⬪AML⬪
#include "pch.h"
#include "singleton.h"

#include "exception.h"
#include "thread.h"

using namespace util;

thread::CriticalSection* SingletonHolder::s_pLock = nullptr;
volatile bool SingletonHolder::s_IsFinalizing = false;
SingletonHolder::Item* SingletonHolder::s_pItem = nullptr;
std::vector<void*>* SingletonHolder::s_pObjStack = nullptr;

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::KillAll()
{
	Initialize();
	// Так как KillAll может быть вызван любым потоком (включая ситуацию, когда другой поток в это
	// же время инициализирует какой-либо синглтон), мы должны защитить доступ к списку s_pItem
	thread::CriticalSection::Lock lock(s_pLock);

	for (Item* p = s_pItem; p;)
	{
		p->fn();
		Item* pOld = p;
		p = p->pNext;
		delete pOld;
	}
	s_pItem = nullptr;
}

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::Initialize()
{
	size_t spinC = 0;
	uint8_t current = 0;
	static std::atomic<uint8_t> s_ILock;
	while (!s_ILock.compare_exchange_weak(current, 1, std::memory_order_acquire))
	{
		// Значение 2 означает, что другой поток уже закончил инициализацию
		if (current == 2)
			return;

		// Так как мы ещё не можем обращаться к другим синглтонам (в т.ч. к SystemInfo, чтобы узнать
		// количество логических процессоров в системе), то после 1000 попыток захвата спинлока отдаём
		// системе остаток тайм-слайса нашего потока (на случай, если процессор в системе всего один)
		if (++spinC == 1000)
		{
			spinC = 0;
			thread::Sleep(0);
		} else
			thread::CPUPause();

		current = 0;
	}

	static uint8_t data[sizeof(thread::CriticalSection)];
	// Инициализируем объект критической секции. Благодаря
	// placement new, этот объект никогда не будет разрушен
	s_pLock = new(data) thread::CriticalSection;

	s_ILock.store(2, std::memory_order_release);
}

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::Finalize()
{
	if (!s_IsFinalizing)
	{
		// Значение s_IsFinalizing равно false и деинициализация только начинается. Так как другой
		// поток сейчас может выполнять инициализацию синглтона, мы не должны допустить установку
		// флага s_IsFinalizing после захвата критической секции другим потоком. Перед захватом
		// критической секции убедимся, что она проинициализирована
		Initialize();

		// Захватываем критическую секцию. Пытаясь это сделать, мы дождемся завершения инициализации
		// синглтона другим потоком (если секция была им захвачена) и гарантируем, что другой поток
		// не начнёт инициализацию снова до того, как мы установим флаг
		thread::CriticalSection::Lock lock(s_pLock);

		s_IsFinalizing = true;
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::SetKiller(KillerFn fn)
{
	// Мы не захватываем здесь критическую секцию, так как вызов этой функции делается только
	// из функции Singleton::CreateInstance, в которой к этому моменту секция уже захвачена.
	// Вышенаписанное справедливо и для функций PushObject/PopObject

	if (fn)
	{
		Item* p = new Item { fn, s_pItem };
		s_pItem = p;
	}

	static bool didOnce = false;
	if (!didOnce)
	{
		atexit([]() {
			Finalize();
			KillAll();
		});
		didOnce = true;
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool SingletonHolder::PushObject(void* pObjPtr)
{
	if (s_pObjStack)
	{
		// Проверим, нет ли в стеке указателя на инициализируемый синглтон. Если он там есть,
		// значит имеется рекурсия. Причём, если этот указатель на вершине стека, значит это
		// внутренняя рекурсия в конструкторе синглтона на самого себя. Если указатель будет
		// найден не на вершине стека, значит у нас рекурсия между 2 различными синглтонами
		for (size_t i = 0; i < s_pObjStack->size(); ++i)
		{
			if ((*s_pObjStack)[i] == pObjPtr)
			{
				bool isNotLast = i < s_pObjStack->size() - 1;
				LogErrorAndAbort(isNotLast ? "Detected cross-singleton recursion" :
					"Detected recursion during singleton initialization");
			}
		}
	} else
		s_pObjStack = new std::vector<void*>;

	// Поместим новый указатель на вершину стека
	s_pObjStack->push_back(pObjPtr);
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::PopObject()
{
	if (s_pObjStack && s_pObjStack->size())
	{
		s_pObjStack->pop_back();
		if (s_pObjStack->empty())
		{
			delete s_pObjStack;
			s_pObjStack = nullptr;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------
void SingletonHolder::LogErrorAndAbort(const char* pErrorMsg)
{
	// TODO: возможно, если бы у нас был журнал (синглтон системного журнала), то мы захотели бы
	// залогировать ошибку, или хотя бы вывести информацию о ней в консоль отладчика (которого у
	// нас пока тоже нет). Поэтому пока просто выбросим исключение
	throw util::EAssertion(pErrorMsg);
}
