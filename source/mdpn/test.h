//∙MDPN
#pragma once

#include <core/singleton.h>
#include <core/util.h>

#include <functional>
#include <list>
#include <map>
#include <string>
#include <type_traits>

//----------------------------------------------------------------------------------------------------------------------
class Test
{
	AML_NONCOPYABLE(Test)

public:
	Test() = default;
	virtual ~Test() = default;

	// Эта функция в классе-наследнике должна возвращать уникальное имя теста
	static std::string GetId() { return ""; }
	// Тесты, перечисленные (через запятую или точку с запятой) в строке, возвращаемой этой функцией,
	// будут выполнены перед тестом (но в случае перекрёстной зависимости тестов это условие может быть
	// нарушено, а часть перечисленных тестов, зависимых от этого теста, может быть выполнена после него)
	static std::string GetPrerequisites() { return ""; }

	virtual bool Execute() = 0;
	// Включает/выключает режим подробного вывода
	void SetVerbose(bool value) { m_VerboseOutput = value; }

protected:
	virtual bool IsCancelled() const;

	virtual std::string GetPrintedName() const { return "SampleTest"; }
	virtual std::string GetPrintedHeader() const;

	// Выводит сообщение об ошибке и возвращает false. Опциональный параметр errorCode
	// позволяет также вывести код ошибки (для облегчения поиска нужного места в коде)
	bool OnError(unsigned errorCode = 0);

	void PrintHeader() const;
	void PrintFooter() const;

	bool m_VerboseOutput = false;
};

//----------------------------------------------------------------------------------------------------------------------
class TestFacility : public util::Singleton<TestFacility>
{
public:
	static constexpr size_t ENDLESS = ~size_t(0);
	using TestFn = std::function<bool()>;

	// Выполняет перечисленные в tests тесты. Параметр repeatC задаёт количество повторений (при значении
	// repeatC равном ENDLESS - бесконечно). Разделители списка tests: запятая и точка с запятой. Если при
	// выполнении теста его функция вернёт false, то тестирование будет прервано и функция Run вернёт false
	bool Run(const std::string& tests, size_t repeatC = 1);
	// Включает/выключает режим подробного вывода в тестах. Значение value в дальнейшем будет передаваться
	// запускаемым тестам через вызов их функции SetVerbose непосредственно перед вызовом функции Execute
	void SetVerbose(bool value) { m_VerboseOutput = value; }

	// Регистрирует тест-функцию fn с именем id. Если тест с таким именем уже
	// существует и replace == true, то указанная функция fn заменит существующий тест
	static void Register(const std::string& id, const TestFn& fn, bool replace = true);

	// Регистрирует класс теста T. Если тест с таким именем уже существует
	// и replace == true, то указанный класс T заменит существующий тест
	template<class T>
	static void Register(bool replace = true)
	{
		static_assert(std::is_base_of<Test, T>::value, "Incorrect class type");
		Instance().AddTest(T::GetId(), T::GetPrerequisites(), []() { return ExecuteTest(new T); }, replace);
	}

	// Проверяет соместимость системы и выполняет другие обязательные проверки. Если параметр
	// enableOutput равен true, то вывод диагностических сообщений в консоль будет разрешён
	static bool CheckRequirements(bool enableOutput = false);

protected:
	TestFacility() = default;
	virtual ~TestFacility() override = default;

private:
	struct TestInfo {
		std::string prerequisites;
		TestFn fn;
	};

	bool AddTest(const std::string& id, const std::string& prerequisites, const TestFn& fn, bool replace);
	bool MakeQueue(std::list<TestFn>& queue, const std::string& tests) const;
	static bool ExecuteTest(Test* pTest);

	// Ищет подстроку pMask в строке pText. Строка pMask может содержать символы маски (? и *). Если параметр
	// exact == true, то ищется полное совпадение строк. Если exact == false, то несколько символов в начале
	// строки pText могут быть пропущены, если оставшаяся её часть полностью совпадает с подстрокой pMask.
	// Если совпадение найдено, то функция вернёт позицию начала подстроки в строке, иначе вернёт -1
	static int FindMatchWithMask(const char* pMask, const char* pText, bool exact);

	static bool TestSprintf(bool enableOutput = false);
	static bool TestFormat(bool enableOutput = false);
	static bool TestSSSE3(bool enableOutput = false);

	std::map<std::string, TestInfo> m_Tests;
	bool m_VerboseOutput = false;
};
