//∙MDPN
#include "pch.h"
#include "test.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/strformat.h>

#include <intrin.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Test
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool Test::IsCancelled() const
{
	return util::SystemConsole::Instance().IsCtrlCPressed();
}

//----------------------------------------------------------------------------------------------------------------------
std::string Test::GetPrintedHeader() const
{
	return util::Format("Running test #9#%s#7", GetPrintedName().c_str());
}

//----------------------------------------------------------------------------------------------------------------------
bool Test::OnError(unsigned errorCode)
{
	aux::Printf(errorCode ? "%s: #12failed (%u)\n" : "%s: #12failed\n",
		m_VerboseOutput ? "  #14Result" : "\b\b\b", errorCode);
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
void Test::PrintHeader() const
{
	aux::Printf(m_VerboseOutput ? "%s...\n" : "%s...", GetPrintedHeader().c_str());
}

//----------------------------------------------------------------------------------------------------------------------
void Test::PrintFooter() const
{
	if (!m_VerboseOutput)
		aux::Printc(IsCancelled() ? "\n" : "\b\b\b: #10ok\n");
	else if (!IsCancelled())
		aux::Printc("  #14Result: #10ok\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestFacility
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::Run(const std::string& tests, size_t repeatC)
{
	std::list<TestFn> queue;
	if (!MakeQueue(queue, tests))
		return false;

	if (!queue.empty())
	{
		while (repeatC)
		{
			for (const auto& fn : queue)
			{
				if (util::SystemConsole::Instance().IsCtrlCPressed())
					return true;
				if (fn && !fn())
					return false;
			}
			if (repeatC != ENDLESS)
				--repeatC;
		}
	}
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void TestFacility::Register(const std::string& id, const TestFn& fn, bool replace)
{
	Instance().AddTest(id, std::string(), fn, replace);
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::CheckRequirements(bool enableOutput)
{
	if (!util::CheckMinimalRequirements(false) || !TestSSSE3(enableOutput))
		return false;

	return TestSprintf(enableOutput) && TestFormat(enableOutput);
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::AddTest(const std::string& id, const std::string& prerequisites, const TestFn& fn, bool replace)
{
	auto testId = util::Trim(id);
	if (!testId.empty())
	{
		util::LoCaseInplace(testId, true);
		auto it = m_Tests.find(testId);
		if (it != m_Tests.end())
		{
			if (replace)
			{
				it->second.prerequisites = prerequisites;
				it->second.fn = fn;
				return true;
			}
		}
		else if (fn)
		{
			m_Tests[testId] = TestInfo { prerequisites, fn };
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::MakeQueue(std::list<TestFn>& queue, const std::string& tests) const
{
	std::set<std::string> all;
	std::function<bool(const std::string&, std::list<TestFn>::const_iterator)> Add;
	auto Insert = [&](const std::string& id, const TestInfo& item, std::list<TestFn>::const_iterator it) {
		if (!all.insert(id).second)
			return true;
		auto newIt = queue.insert(it, item.fn);
		return item.prerequisites.empty() || Add(item.prerequisites, newIt);
	};
	Add = [&](const std::string& ids, std::list<TestFn>::const_iterator it) {
		for (auto& id : util::Split(ids, ",;"))
		{
			bool found = false;
			util::LoCaseInplace(id, true);
			auto testIt = m_Tests.find(id);
			if (testIt != m_Tests.end())
			{
				found = true;
				if (!Insert(id, testIt->second, it))
					return false;
			}
			else if (id.find_first_of("?*") != std::string::npos)
			{
				for (const auto& item : m_Tests)
				{
					if (FindMatchWithMask(id.c_str(), item.first.c_str(), true) >= 0)
					{
						found = true;
						if (!Insert(item.first, item.second, it))
							return false;
					}
				}
			}
			if (!found)
			{
				aux::Printf("Test \"%s\" not found\n", id.c_str());
				return false;
			}
		}
		return true;
	};
	return Add(tests, queue.end());
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::ExecuteTest(Test* pTest)
{
	if (pTest)
	{
		try
		{
			std::unique_ptr<Test> p(pTest);
			pTest->SetVerbose(Instance().m_VerboseOutput);
			return pTest->Execute();
		}
		catch (...)
		{
			aux::Print("\rTest failed: unhandled c++ exception\n");
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
int TestFacility::FindMatchWithMask(const char* pMask, const char* pText, bool exact)
{
	const size_t maskLen = pMask ? strlen(pMask) : 0;
	const size_t textLen = pText ? strlen(pText) : 0;
	if (maskLen && textLen)
	{
		// p (pos) - позиция начала совпадения
		// tp (textPos) - текущая позиция в pText
		// mp (maskPos) - текущая позиция в pMask
		for (size_t p = 0, tp = 0, mp = 0;;)
		{
			if (tp == textLen)
			{
				while (pMask[mp] == '*') ++mp;
				return (mp == maskLen) ? static_cast<int>(p) : -1;
			}
			if (mp < maskLen)
			{
				if (pMask[mp] == '*')
				{
					while (pMask[++mp] == '*');
					if (mp == maskLen)
						return static_cast<int>(p);
					int r = FindMatchWithMask(pMask + mp, pText + tp, false);
					if (exact)
						return (r >= 0) ? 0 : -1;
					if (r >= 0)
						return static_cast<int>(p);
				}
				else if (pMask[mp] == '?' || pText[tp] == pMask[mp])
				{
					++tp;
					++mp;
					continue;
				}
				else if (exact)
					break;
			}
			tp = ++p;
			mp = 0;
		}
	}
	return -1;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::TestSprintf(bool enableOutput)
{
	char buffer[24];

	// Тестируем "%llu"
	for (int i = 0; i < 64; ++i)
	{
		const uint64_t n = 1ull << i;
		memset(buffer, 0, sizeof(buffer));
		const int len = sprintf_s(buffer, "%llu", n);

		uint64_t v = 0, k = 1;
		for (int j = 0; j < len; ++j, k *= 10)
			v += k * (buffer[len - j - 1] - '0');
		if (n != v)
		{
			if (enableOutput)
			{
				aux::Printf("TestFacility.TestSprintf(%%llu) failed: i=%d, %s\n", i, buffer);
			}
			return false;
		}
	}

	// Тестируем "%llX"
	for (int i = 0; i < 64; ++i)
	{
		const uint64_t n = (1ull << i) + i;
		memset(buffer, 0, sizeof(buffer));
		const int len = sprintf_s(buffer, "%llX", n);

		uint64_t v = 0;
		for (int j = 0; j < len; ++j)
		{
			const char c = buffer[j];
			v = (v << 4) + c - ((c <= '9') ? 48 : 55);
		}
		if (n != v)
		{
			if (enableOutput)
			{
				aux::Printf("TestFacility.TestSprintf(%%llX) failed: i=%d, %s\n", i, buffer);
			}
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::TestFormat(bool enableOutput)
{
	std::string s;

	// Тестируем "%llu"
	for (int i = 0; i < 64; ++i)
	{
		const uint64_t n = 1ull << i;
		s = util::Format("%llu", n);
		const size_t len = s.size();

		uint64_t v = 0, k = 1;
		for (size_t j = 0; j < len; ++j, k *= 10)
			v += k * (s[len - j - 1] - '0');
		if (n != v)
		{
			if (enableOutput)
			{
				aux::Printf("TestFacility.TestFormat(%%llu) failed: i=%d, %s\n", i, s.c_str());
			}
			return false;
		}
	}

	// Тестируем "%llX"
	for (int i = 0; i < 64; ++i)
	{
		const uint64_t n = (1ull << i) + i;
		s = util::Format("%llX", n);
		const size_t len = s.size();

		uint64_t v = 0;
		for (size_t j = 0; j < len; ++j)
		{
			const char c = s[j];
			v = (v << 4) + c - ((c <= '9') ? 48 : 55);
		}
		if (n != v)
		{
			if (enableOutput)
			{
				aux::Printf("TestFacility.TestFormat(%%llX) failed: i=%d, %s\n", i, s.c_str());
			}
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestFacility::TestSSSE3(bool enableOutput)
{
	int cpuInfoA[4];
	__cpuid(cpuInfoA, 0);
	if (cpuInfoA[0] >= 0x01)
	{
		__cpuidex(cpuInfoA, 0x01, 0);

		bool SSE3 = (cpuInfoA[2] & 0x0001) > 0;
		bool SSSE3 = (cpuInfoA[2] & 0x0200) > 0;

		if (SSE3 && SSSE3)
			return true;
	}

	if (enableOutput)
	{
		aux::Printf("TestFacility.TestSSSE3 failed: CPU doesn't support SSSE3\n");
	}
	return false;
}
