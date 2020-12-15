//⬪MDPN⬪
#include "pch.h"
#include "numsettest.h"

#include "util.h"

#include <core/auxutil.h>
#include <core/platform.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   TestNumberSet
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
bool TestNumberSet::Execute()
{
	m_VerboseOutput = true;
	PrintHeader();

	if (!IsCancelled() && !CalcSafeSize())
		return false;
	if (!IsCancelled() && !TestMain())
		return false;

	PrintFooter();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
std::string TestNumberSet::GetPrintedHeader() const
{
	return "Testing validity of #9NumberSet#7";
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumberSet::OnError(unsigned errorCode)
{
	aux::Printf(errorCode ? "\b\b\b: #12failed (%u)\n" : "\b\b\b: #12failed\n", errorCode);
	return Test::OnError();
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumberSet::CalcSafeSize()
{
	aux::Print("  Calculating set's capacity...");

	Number num;
	NumberSet numSet;
	// В первом цикле добавляем в набор ~1/2 чисел от их полного количества в хеш-таблице.
	// Этого будет достаточно, чтобы задействуемый размер хеш-таблицы вышел на максимум
	for (size_t count = 0; count < size_t(1) << (NSHelper::HASH_BITS - 1);)
	{
		num.Set(++count);
		if (!numSet.Insert(num))
			return OnError(1);
	}
	size_t addedC = 0;
	numSet.Clear(false);
	// Очистим набор и повторим процесс снова. В отличие от первого раза теперь хеш-таблица уже не будет
	// расширяться и поэтому при переполнении набора первый блок одной из частей будет удалён полностью,
	// а значение addedC - 1 будет примерно соответствовать предельному количеству элементов в наборе
	while (numSet.GetSize() == addedC)
	{
		num.Set(++addedC);
		if (!numSet.Insert(num))
			return OnError(2);
	}
	// Если ошибок нет, то из набора был удалён ровно один блок
	if (addedC - numSet.GetSize() != NSHelper::CHUNK_SIZE)
		return OnError(3);

	m_SafeSize = addedC - 1;
	aux::Printf("\b\b\b: ~%s\n", SeparateWithCommas(m_SafeSize).c_str());
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumberSet::TestMain()
{
	// Будет 2 фазы по 16 циклов: во 2-й фазе будем использовать тот же контейнер,
	// но очистим его, чтобы убедиться, что после очистки он работает корректно
	for (int phase = 0; !IsCancelled() && phase < 2; ++phase)
	{
		m_MaxSize = 0;
		for (size_t loop = 0; !IsCancelled() && loop < 16; ++loop)
		{
			if (!DoLoop(loop))
				return false;
		}
		m_All.clear();
		m_NumSet.Clear(true);
	}
	aux::Printc(IsCancelled() ? "\n" : "\b\b\b: #10ok\n");
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool TestNumberSet::DoLoop(size_t loop)
{
	Number num;
	size_t addedC = m_All.size();
	const size_t numCount = m_SafeSize / 6;

	PrintProgress();
	// Добавляем в набор numCount новых чисел
	for (size_t numC = 0; numC < numCount;)
	{
		num.Set(m_Rg.UInt());
		if (num && m_All.insert(num).second)
		{
			++numC;
			if (!m_NumSet.Insert(num))
				return OnError(4);
			++addedC;
		}
	}
	const size_t setSize = m_NumSet.GetSize();
	m_MaxSize = (m_MaxSize < setSize) ? setSize : m_MaxSize;
	if ((loop < 5 && setSize != addedC) || m_MaxSize - setSize >= NSHelper::CHUNK_SIZE)
		return OnError(5);

	if (IsCancelled())
		return true;

	PrintProgress();
	// Проверяем наличие всех добавленных чисел в наборе
	HashSet::const_iterator ite = m_All.end();
	for (HashSet::iterator it = m_All.begin(); it != ite;)
	{
		num = *it;
		if (m_NumSet.Exists(num))
			++it;
		else if (loop < 5)
			return OnError(6);
		else
		{
			it = m_All.erase(it);
			--addedC;
		}
	}
	if (m_NumSet.GetSize() != addedC)
		return OnError(7);

	if (IsCancelled())
		return true;

	PrintProgress();
	// Проверяем отсутствие лишних чисел в наборе
	for (size_t numC = 0; numC < numCount;)
	{
		num.Set(m_Rg.UInt());
		if (m_All.find(num) == ite)
		{
			++numC;
			if (m_NumSet.Exists(num))
				return OnError(8);
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void TestNumberSet::PrintProgress()
{
	aux::Printf("\r  Performing step %2u/96...", ++m_Loop);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   EndlessTestNumberSet
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
std::string EndlessTestNumberSet::GetPrintedHeader() const
{
	return "Running continuous test of #9NumberSet#7";
}

//----------------------------------------------------------------------------------------------------------------------
bool EndlessTestNumberSet::TestMain()
{
	for (size_t loop = 0; !IsCancelled(); ++loop)
	{
		if (!DoLoop(loop))
			return false;
	}
	aux::Print("\n");
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
void EndlessTestNumberSet::PrintProgress()
{
	if (m_Loop % 3 == 0)
	{
		uint64_t it = 1 + m_Loop / 3;
		aux::Printf("\r  Performing test iteration %s...", SeparateWithCommas(it).c_str());
	}
	++m_Loop;
}
