//⬪MDPN⬪
#pragma once

#include <core/util.h>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Mode - режим работы программы (базовый класс)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class Mode
{
	AML_NONCOPYABLE(Mode)

public:
	Mode() = default;
	virtual ~Mode() = default;

	// Создаёт дефолтный объект Mode, функция Run которого выводит сообщение
	// о том, что команда (первый параметр в командной строке) не распознана
	static std::unique_ptr<Mode> Create(int argC, const wchar_t* argA[]);

	// Возвращает новый объект типа T (конкретный режим работы программы),
	// перемещая в него содержимое вектора с параметрами командной строки
	template<class T> std::unique_ptr<Mode> Expand()
	{
		static_assert(std::is_base_of<Mode, T>::value, "Incorrect type");
		std::unique_ptr<Mode> obj = std::make_unique<T>();
		std::swap(obj->m_Params, m_Params);
		return obj;
	}

	// Возвращает true, если первый параметр командной строки совпадает (с точностью
	// до регистра букв) с pCmd или если командная строка пуста и optional == true
	bool IsCommand(const char* pCmd, bool optional = false) const;

	// Выполняет основную работу
	virtual bool Run();

protected:
	void SetParams(int argC, const wchar_t* argA[]);

	void OnCmdNotRecognized() const;
	void OnInvalidCmdLine() const;

	// Параметры командной строки
	std::vector<std::string> m_Params;
};
