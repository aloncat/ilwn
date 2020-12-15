//⬪MDPN⬪
#pragma once

#include "number.h"
#include "numset.h"
#include "test.h"

#include <core/randgen.h>

#include <string>
#include <unordered_set>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Validity.NumberSet - тест корректности работы класса NumberSet
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class TestNumberSet : public Test
{
public:
	static std::string GetId() { return "Test.Validity.NumberSet"; }
	static std::string GetPrerequisites() { return "Test.Validity.Number, Test.Validity.FixNumber"; }

	virtual bool Execute() override;

protected:
	using HashSet = std::unordered_set<FixNumber, FixNumber::Hasher>;

	class NSHelper : NumberSet {
	public:
		static constexpr size_t HASH_BITS = NumberSet::HASH_BITS;
		static constexpr size_t CHUNK_SIZE = NumberSet::CHUNK_SIZE;
	};

	virtual std::string GetPrintedHeader() const override;
	bool OnError(unsigned errorCode = 0);

	bool CalcSafeSize();
	virtual bool TestMain();
	bool DoLoop(size_t loop);
	virtual void PrintProgress();

	HashSet m_All;
	NumberSet m_NumSet;
	math::RandGen m_Rg;
	size_t m_SafeSize = 0;
	size_t m_MaxSize = 0;
	size_t m_Loop = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test.Endless.NumberSet - тест корректности работы класса NumberSet (продолжительный режим)
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
class EndlessTestNumberSet : public TestNumberSet
{
public:
	static std::string GetId() { return "Test.Endless.NumberSet"; }
	static std::string GetPrerequisites() { return "Test.Validity.NumberSet"; }

protected:
	virtual std::string GetPrintedHeader() const override;

	virtual bool TestMain() override;
	virtual void PrintProgress() override;
};
