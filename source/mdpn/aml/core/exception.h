//⬪AML⬪
#pragma once

#include <exception>
#include <string>

namespace util {

//----------------------------------------------------------------------------------------------------------------------
class EGeneric : public std::exception
{
public:
	EGeneric() noexcept;
	explicit EGeneric(const char* pMsg) noexcept;
	EGeneric(const EGeneric& that) noexcept;
	virtual ~EGeneric() override;

	virtual const char* what() const noexcept override;
	virtual const char* ClassName() const noexcept = 0;

	EGeneric& operator =(const EGeneric& that) noexcept;

private:
	void Tidy();
	// Выделяет массив достаточного размера, копирует в него
	// строку pStr и возвращает указатель на этот массив
	static const char* CopyString(const char* pStr);

	const char* m_pWhat;
};

//----------------------------------------------------------------------------------------------------------------------
#define AML_EXCEPTION(NAME, ANCESTOR) \
public: \
	NAME() noexcept : ANCESTOR(#NAME) {} \
	explicit NAME(const char* pMsg) noexcept : ANCESTOR(pMsg) {} \
	explicit NAME(const std::string& msg) noexcept : ANCESTOR(msg.c_str()) {} \
	virtual const char* ClassName() const noexcept override { return "class util::" # NAME; }

//----------------------------------------------------------------------------------------------------------------------
class ELogic : public EGeneric
{
	AML_EXCEPTION(ELogic, EGeneric)
};

//----------------------------------------------------------------------------------------------------------------------
class EAssertion : public ELogic
{
	AML_EXCEPTION(EAssertion, ELogic)
};

//----------------------------------------------------------------------------------------------------------------------
class ERuntime : public EGeneric
{
	AML_EXCEPTION(ERuntime, EGeneric)
};

//----------------------------------------------------------------------------------------------------------------------
class EHalt : public ERuntime
{
	AML_EXCEPTION(EHalt, ERuntime)
};

} // namespace util
