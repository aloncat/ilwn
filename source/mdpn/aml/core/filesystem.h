//⬪AML⬪
#pragma once

#include "platform.h"

#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
struct FileSystem
{
	static const wchar_t* const DELIMITER;

	// Возвращает полный (абсолютный) путь к указанному файлу (директории),
	// исправляет слеши в пути на правильные, убирая лишние повторения
	static std::wstring GetFullPath(const std::wstring& path);
	// Возвращает путь path, из которого удалены все концевые слеши
	// кроме слеша в начале строки и слеша, следующего за двоеточием
	static std::wstring RemoveTrailingSlashes(const std::wstring& path);

	// Возвращает путь path, в котором для имени файла (последней директории) расширение заменено на newExtension.
	// Если newExtension это пустая строка, то функция удаляет расширение файла (директории), оставляя только имя.
	// Значение path может быть относительным или абсолютным путем и не должно оканчиваться слешем или двоеточием
	// (если путь path оканчивается слешем или двоеточием, то функция вернёт строку path без изменений)
	static std::wstring ChangeExtension(const std::wstring& path, const std::wstring& newExtension);

	// Извлекает из указанного пути path путь к файлу (последней директории в указанном пути). Возвращаемый путь
	// всегда оканчивается слешем, кроме случая, когда path имеет вид "C:myfile.ext" (тогда функция вернёт "C:").
	// Если указанный путь содержит только имя файла (директории) без слешей, то функция вернёт пустую строку
	static std::wstring ExtractPath(const std::wstring& path);
	// Извлекает из указанного пути path имя файла (директории)
	// с расширением. Путь path не должен оканчиваться слешем
	static std::wstring ExtractFilename(const std::wstring& path);
	// Извлекает из указанного пути path расширение файла
	// (директории). Путь path не должен оканчиваться слешем
	static std::wstring ExtractExtension(const std::wstring& path);

	// Проверяет существование указанного файла или
	// директории. Параметр path не поддерживает маску
	static bool FileExists(const std::wstring& path);
	static bool DirectoryExists(const std::wstring& path);

protected:
	static int GetAttributes(const std::wstring& path);

	#if AML_OS_WINDOWS
	// Если размер строки path менее MAX_PATH символов, то функция возвращает path.c_str(). Если длина пути
	// path равна или превышает указанное значение и путь не является UNC путём, то строке tmp присваивается
	// значение GetFullPath(path), дополненное префиксом "\\?\", и функция возвращает tmp.c_str()
	static const wchar_t* MakeLongPath(const std::wstring& path, std::wstring& tmp);
	#endif
};

} // namespace util
