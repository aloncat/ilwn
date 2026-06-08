//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/util.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Mode - режим работы программы (базовый класс)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Mode
{
	AML_NONCOPYABLE(Mode)

public:
	class Helper;

	Mode() = default;
	virtual ~Mode() = default;

	// Выполняет основную работу
	virtual bool Run();

protected:
	void OnCmdNotRecognized() const;
	void OnInvalidCmdOptions() const;
	void OnInvalidCmdLine() const;

protected:
	// Параметры командной строки
	std::vector<std::string> m_Params;
};

//--------------------------------------------------------------------------------------------------------------------------------
class Mode::Helper final
{
	AML_NONCOPYABLE(Helper)

public:
	// Создаёт дефолтный объект Mode, функция Run которого выводит сообщение
	// о том, что команда (первый параметр в командной строке) не распознана
	Helper(int argCount, const wchar_t* args[]);

	// Возвращает новый объект типа T (конкретный режим работы программы),
	// перемещая в него содержимое вектора с параметрами командной строки
	template<class T> void Expand()
	{
		static_assert(std::is_base_of_v<Mode, T>, "Incorrect type");
		SetNewMode(std::make_unique<T>());
	}

	// Возвращает true, если первый параметр командной строки совпадает (с точностью
	// до регистра букв) с pCmd или если командная строка пуста и optional == true
	bool IsCommand(std::string_view cmd, bool optional = false) const;

	bool Run();

private:
	void SetNewMode(std::unique_ptr<Mode>&& newMode);

private:
	std::unique_ptr<Mode> m_Mode;
};
