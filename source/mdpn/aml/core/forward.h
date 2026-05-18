//⬪AML⬪
#pragma once

// NB: в forward.h не добавляются классы синглтонов, так как это не имеет смысла: не должно быть
// ситуации, когда нам нужно хранить указатель или ссылку на синглтон в переменной (члене данных),
// объявленной в заголовочном файле. Также сюда не добавляются никакие шаблонные классы во
// избежание потенциальных проблем со значениями по умолчанию их параметров

namespace math {
	class RandGen;
}

namespace thread {
	class CriticalSection;
}

namespace util {
	// Классы исключений
	class EAssertion;
	class EGeneric;
	class EHalt;
	class ELogic;
	class ERuntime;
	// Классы файлов
	class BinaryFile;
	class File;
	// Разное
	class Console;
	class FuncToggle;
	class MemoryWriter;
}
