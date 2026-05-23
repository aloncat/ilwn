//⬪AML⬪
#pragma once

#include "platform.h"
#include "util.h"

#include <string.h>
#include <type_traits>

namespace util {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DynamicArray
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс DynamicArray - простейший контейнер, представляет собой массив элементов одного
// типа. Массив заданного размера выделяется в куче при создании объекта. Контейнер
// может быть использован только для простых типов без инициализатора (POD-types)

//----------------------------------------------------------------------------------------------------------------------
template<class T>
class DynamicArray final
{
	AML_NONCOPYABLE(DynamicArray)
	static_assert(std::is_pod<T>::value, "Only POD-types are allowed");

public:
	// Параметр size задаёт желаемый размер контейнера (количество элементов). Если параметр
	// dataA задан, то элементы контейнера будут проинициализированы значениями из массива dataA
	explicit DynamicArray(size_t size, const T* dataA = nullptr)
		: m_ItemA(size ? new Item[size] : nullptr)
	{
		if (dataA && size)
			memcpy(m_ItemA, dataA, size * sizeof(T));
	}

	~DynamicArray()
	{
		if (m_ItemA)
			delete[] m_ItemA;
	}

	operator T*()
	{
		return reinterpret_cast<T*>(m_ItemA);
	}

	operator const T*() const
	{
		return reinterpret_cast<T*>(m_ItemA);
	}

private:
	using Item = uint8_t[sizeof(T)];

	Item* m_ItemA;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SmartArray
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс SmartArray - простой контейнер, представляет собой массив элементов одного типа. Его главное
// отличие от DynamicArray в том, что этот контейнер имеет внутренний буфер. Если запрошенный размер
// (параметр size конструктора) меньше или равен значению capacity (по умолчанию 80), то массив для
// элементов будет размещён во внутреннем буфере. Иначе он будет выделен в куче. Контейнер может
// быть использован только для простых типов без инициализатора (POD-types)

//----------------------------------------------------------------------------------------------------------------------
template<class T, size_t capacity = 80>
class SmartArray final
{
	AML_NONCOPYABLE(SmartArray)
	static_assert(std::is_pod<T>::value, "Only POD-types are allowed");

public:
	// Параметр size задаёт желаемый размер контейнера (количество элементов). Если параметр
	// dataA задан, то элементы контейнера будут проинициализированы значениями из массива dataA
	explicit SmartArray(size_t size, const T* dataA = nullptr)
		: m_ItemA((size <= capacity) ? m_LocalA : new Item[size])
	{
		if (dataA && size)
			memcpy(m_ItemA, dataA, size * sizeof(T));
	}

	~SmartArray()
	{
		if (m_ItemA != m_LocalA)
			delete[] m_ItemA;
	}

	operator T*()
	{
		return reinterpret_cast<T*>(m_ItemA);
	}

	operator const T*() const
	{
		return reinterpret_cast<T*>(m_ItemA);
	}

private:
	using Item = uint8_t[sizeof(T)];

	Item* m_ItemA;
	Item m_LocalA[capacity];
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FlexibleArray
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс FlexibleArray - это контейнер, представляющий собой массив элементов одного типа. От DynamicArray и
// SmartArray этот контейнер отличает то, что он может быть проинициализирован пользовательским массивом заданного
// размера. Также, он может менять свой размер в сторону увеличения: в этом случае новый массив большего размера
// всегда выделяется в куче. Контейнер может быть использован только для простых типов без инициализатора

//----------------------------------------------------------------------------------------------------------------------
template<class T>
class FlexibleArray final
{
	AML_NONCOPYABLE(FlexibleArray)
	static_assert(std::is_pod<T>::value, "Only POD-types are allowed");

public:
	// Инициализирует контейнер размером size элементов. Если значение параметра
	// size больше 0, то массив соответствующего размера сразу выделяется в куче
	explicit FlexibleArray(size_t size = 0)
		: m_pBuffer(size ? new Item[size] : nullptr)
		, m_pExternal(nullptr)
		, m_Size(size)
	{
	}

	// Инициализирует контейнер пользовательским массивом pUserBuffer размером userSize
	// элементов. Этот массив будет использован для хранения элементов контейнера
	FlexibleArray(T* pUserBuffer, size_t userSize)
		: m_pBuffer(reinterpret_cast<Item*>(pUserBuffer))
		, m_pExternal(pUserBuffer)
		, m_Size(pUserBuffer ? userSize : 0)
	{
	}

	template<size_t userSize>
	explicit FlexibleArray(T (&pUserBuffer)[userSize])
		: m_pBuffer(reinterpret_cast<Item*>(pUserBuffer))
		, m_pExternal(pUserBuffer)
		, m_Size(pUserBuffer ? userSize : 0)
	{
	}

	~FlexibleArray()
	{
		if (m_pBuffer && !m_pExternal)
			delete[] m_pBuffer;
	}

	// Реинициализирует контейнер новым пользовательским
	// массивом pUserBuffer размером userSize элементов
	void Set(T* pUserBuffer, size_t userSize)
	{
		if (m_pBuffer && !m_pExternal)
			delete[] m_pBuffer;
		m_pBuffer = reinterpret_cast<Item*>(pUserBuffer);
		m_pExternal = pUserBuffer;
		m_Size = pUserBuffer ? userSize : 0;
	}

	template<size_t userSize>
	void Set(T (&pUserBuffer)[userSize])
	{
		Set(pUserBuffer, userSize);
	}

	size_t GetSize() const
	{
		return m_Size;
	}

	// Увеличивает (при необходимости) размер контейнера до newSize элементов. Если значение newSize
	// меньше или равно текущему размеру контейнера, то ничего не меняется. Если retainContents
	// равен true, то при увеличении размера контейнера его содержимое будет сохранено
	void Grow(size_t newSize, bool retainContents = false)
	{
		if (newSize > m_Size)
		{
			if (retainContents)
				Resize(newSize);
			else
				Reallocate(newSize);
		}
	}

	operator T*()
	{
		return reinterpret_cast<T*>(m_pBuffer);
	}

	operator const T*() const
	{
		return reinterpret_cast<T*>(m_pBuffer);
	}

private:
	using Item = uint8_t[sizeof(T)];

	AML_NOINLINE void Reallocate(size_t newSize)
	{
		if (m_pBuffer && !m_pExternal)
			delete[] m_pBuffer;
		m_pExternal = m_pBuffer = nullptr;
		m_Size = 0;

		m_pBuffer = new Item[newSize];
		m_Size = newSize;
	}

	AML_NOINLINE void Resize(size_t newSize)
	{
		Item* pNew = new Item[newSize];
		if (m_Size)
		{
			memcpy(pNew, m_pBuffer, m_Size * sizeof(T));
			if (!m_pExternal)
				delete[] m_pBuffer;
		}
		m_pBuffer = pNew;
		m_pExternal = nullptr;
		m_Size = newSize;
	}

	Item* m_pBuffer;
	void* m_pExternal;
	size_t m_Size;
};

} // namespace util
