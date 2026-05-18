#pragma once

#include "../../core/console.h"
#include "virtkbd.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class ConsoleOld : Console
{
public:
	enum {
		DISABLE_INPUT	= 0,		// Запретить ввод
		ENABLE_TEXT		= 0x02,		// Разрешить ввод текста
		NO_ECHO			= 0x01,		// Не показывать вводимые символы (только с ENABLE_TEXT)
		ENABLE_VKEYS	= 0x04,		// Разрешить виртуальные клавиши (очередь событий)

		IS_CTRL_DOWN	= VKeyQueue::IS_CTRL_DOWN,
		IS_ALT_DOWN		= VKeyQueue::IS_ALT_DOWN,
		IS_SHIFT_DOWN	= VKeyQueue::IS_SHIFT_DOWN
	};

	// Полностью очищает окно консоли, устанавливает курсор в левый верхний угол
	virtual void Clear();

	// Извлекает в input введенную строку. Если пользователь еще не нажал Enter (т.е. ввод строки
	// не завершен), то функция вернет false, сохранив в input все введенные символы. Если клавиша
	// Enter была нажата, то функция сохранит введенную строку в input, очистит ввод и вернет true.
	// Если параметр clear равен true, то ввод будет очищен в обоих случаях
	bool ReadString(std::wstring& input, bool clear = false);

	// Ожидание ввода. Если completeString равен false, то функция вернет управление сразу же после
	// нажатия любой клавиши. Если true, то только после ввода всей строки (нажатия клавиши Enter)
	void WaitForInput(bool completeString = false);

	// Возвращает количество полных секунд, прошедших с момента последнего нажатия клавиши
	unsigned TimeSinceLastActivity() const;

	void SetInputMode(unsigned flags, bool clearQueue = true);
	bool GetVKeyEvent(unsigned& vKey, unsigned& flags);
};

} // namespace util
