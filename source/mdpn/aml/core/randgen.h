//⬪AML⬪
#pragma once

#include "platform.h"

namespace math {

//----------------------------------------------------------------------------------------------------------------------
class RandGen
{
public:
	RandGen();
	explicit RandGen(unsigned seed);

	// Задаёт новое зерно генератора
	void Seed(unsigned seed);

	// Генерирует псевдо-случайное 32-битное целое число
	unsigned UInt() { return Next(); }
	// Генерирует псевдо-случайное целое число X, такое что 0 <= X < range
	unsigned UInt(unsigned range);
	// Генерирует псевдо-случайное дробное число X, такое что 0.0 <= X <= 1.0
	float Float();

protected:
	uint32_t Next();

	uint32_t m_X, m_Y, m_Z;
};

} // namespace math
