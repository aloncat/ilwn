//⬪AML⬪
#pragma once

#include "util.h"

#include <functional>
#include <utility>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Toggle
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс Toggle - это "переключатель". В момент своей инициализации он запоминает значение указанной
// переменной, затем (опционально) присваивает ей новое значение. При разрушении объекта Toggle (при
// выходе переменной из области видимости) исходное значение указанной переменной восстанавливается

//----------------------------------------------------------------------------------------------------------------------
template<class T>
class Toggle
{
	AML_NONCOPYABLE(Toggle)

public:
	// Инициализирует объект, не меняя значение переменной var
	explicit Toggle(T& var)
		: m_pVar(&var)
		, m_Value(var)
	{
	}

	// Инициализирует объект и меняет значение переменной var на newValue
	Toggle(T& var, const T& newValue)
		: m_pVar(&var)
		, m_Value(std::move(var))
	{
		var = newValue;
	}

	// Инициализирует объект и меняет значение переменной var на newValue
	Toggle(T& var, T&& newValue)
		: m_pVar(&var)
		, m_Value(std::move(var))
	{
		var = std::forward<T>(newValue);
	}

	// Восстанавливает исходное значение переменной
	~Toggle()
	{
		*m_pVar = std::move(m_Value);
	}

	// Возвращает исходное значение переменной
	const T& GetOriginal() const
	{
		return m_Value;
	}

	// Задаёт новое "исходное" значение для переменной. Теперь при разрушении объекта
	// переменной будет присвоенно значение newRestoreValue. Если параметр setNow равен
	// true, то значение самой переменной также будет изменено на newRestoreValue
	void SetRestore(const T& newRestoreValue, bool setNow = false)
	{
		m_Value = newRestoreValue;
		if (setNow)
			*m_pVar = m_Value;
	}

	// Функция аналогична предыдущей (но использует R-value)
	void SetRestore(T&& newRestoreValue, bool setNow = false)
	{
		m_Value = std::forward<T>(newRestoreValue);
		if (setNow)
			*m_pVar = m_Value;
	}

	// Восстанавливает исходное значение переменной
	void Restore()
	{
		*m_pVar = m_Value;
	}

	// Возвращает ссылку на переменную
	T& operator *() { return *m_pVar; }
	const T& operator *() const { return *m_pVar; }

	// Позволяет обратиться к члену переменной (если переменная - это класс или структура)
	T* operator ->() { return m_pVar; }
	const T* operator ->() const { return m_pVar; }

protected:
	T* m_pVar;
	T m_Value;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FuncToggle
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс FuncToggle - это "переключатель" с пользовательской функцией. В момент своей
// инициализации и разрушения он вызывает заданный (при вызове конструктора) функтор

//----------------------------------------------------------------------------------------------------------------------
class FuncToggle
{
	AML_NONCOPYABLE(FuncToggle)

public:
	// Прототип функции, которая будет вызываться конструктором и деструктором. При вызове функции из
	// конструктора bool параметр будет равен true, а при вызове из деструктора - false. Если функция
	// вернёт true, она будет снова вызвана из деструктора, если вернёт false, то не будет
	using ToggleFn = std::function<bool(bool)>;

	FuncToggle(const ToggleFn& fn)
		: m_Fn(fn)
	{
		if (m_Fn && !m_Fn(true))
			m_Fn = nullptr;
	}

	FuncToggle(ToggleFn&& fn)
		: m_Fn(std::move(fn))
	{
		if (m_Fn && !m_Fn(true))
			m_Fn = nullptr;
	}

	~FuncToggle()
	{
		if (m_Fn)
			m_Fn(false);
	}

	// Вызывает пользовательскую функцию с параметром bool, равным false (так же, как это делает деструктор
	// класса). Если функция вернула false или параметр noMoreRestore был равен true, то пользовательская
	// функция не будет вызвана из деструктора при разрушении объекта. Если пользовательская функция
	// вернула false в первый раз при вызове из конструктора, то Restore её не вызовет
	void Restore(bool noMoreRestore = false)
	{
		if (m_Fn && (!m_Fn(false) || noMoreRestore))
			m_Fn = nullptr;
	}

protected:
	ToggleFn m_Fn;
};

} // namespace util
