//∙MDPN
#include "pch.h"
#include "arch.h"

#include "number.h"
#include "util.h"

#include <auxlib/print.h>
#include <core/console.h>
#include <core/strformat.h>
#include <core/winapi.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch::Base
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
void Arch::Base::Execute()
{
	// Сбросим таймер перед началом работы, чтобы в конце вывести корректное время.
	// Если время бы не выводилось, то сбрасывать таймер было бы не обязательно
	m_Timer.Reset();

	// Здесь сделаем что-то полезное...

	// Выведем время, затраченное на работу
	if (!IsCancelled())
		PrintTimeElapsed();
}

//----------------------------------------------------------------------------------------------------------------------
bool Arch::Base::IsCancelled() const
{
	return util::SystemConsole::Instance().IsCtrlCPressed();
}

//----------------------------------------------------------------------------------------------------------------------
void Arch::Base::PrintTimeElapsed(bool showMs)
{
	uint64_t timeElapsed = m_Timer.GetElapsed() / 1000;
	// Округляем по матем. правилам до секунд
	timeElapsed += showMs ? 0 : 500;

	size_t hours = static_cast<size_t>(timeElapsed / 3600000);
	size_t remains = static_cast<size_t>(timeElapsed - hours * 3600000ull);

	size_t minutes = remains / 60000;
	remains -= minutes * 60000;
	size_t seconds = remains / 1000;
	size_t ms = remains % 1000;

	std::string s = "CPU time spent: ";
	s += util::Format(hours ? "%u:%02u" : "%u", hours ? hours : minutes, minutes);
	aux::Printf(showMs ? "%s:%02u.%03u\n" : "%s:%02u\n", s.c_str(), seconds, ms);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch::LychrelCoincidence
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
void Arch::LychrelCoincidence::Execute(size_t minRange, size_t maxRange)
{
	// Тистируем, только начиная с 3-значного диапазона, так
	// как в первых двух диапазонах числа Лишрел отсутствуют
	minRange = std::max(size_t(3), minRange);
	maxRange = std::min(maxRange, MAX_RANGE);

	if (minRange > maxRange)
		return;

	// Минимальное кол-во операций RAA для каждого диапазона, достаточное для достоверного
	// определения, является ли некоторое его число отложенным палиндром или числом Лишрел
	static const unsigned maxStepA[1 + MAX_RANGE] = { 0, 2, 24, 24, 24, 55, 64, 96,
		96, 98, 109, 149, 149, 188, 188, 201, 201 };

	m_Timer.Reset();

	for (size_t curRange = minRange; !IsCancelled() && curRange <= maxRange; ++curRange)
	{
		if (curRange > minRange)
			aux::Print("\n");
		if (!CheckRange(curRange, maxStepA[curRange]))
			break;
	}

	if (!IsCancelled())
		PrintTimeElapsed(true);

	// Освобождаем память
	m_SieveSet.Clear(true);
}

//----------------------------------------------------------------------------------------------------------------------
bool Arch::LychrelCoincidence::CheckRange(size_t range, unsigned maxStepC)
{
	ThreadTime timer;
	BigNumber num, n, m;

	for (size_t len = MAX_CONSEQ_LEN; len > range; --len)
	{
		m_SieveSet.Clear(false);

		::Sleep(1);
		timer.Reset();

		size_t siftedC = 0;
		size_t lychrelC = 0;
		uint32_t lastTick = 0;
		size_t sieveSetSize = 0;
		bool isSetSufficient = true;

		num = std::string("1").append(range - 1, '0');
		for (size_t i = 0; num.GetLength() == range; ++i, ++num)
		{
			num.SkipRAADups();

			n = num;
			unsigned stepDoneC;
			if (!n.RAATillLength(len, stepDoneC))
			{
				if (m_SieveSet.Exists(n))
				{
					++siftedC;
					++lychrelC;
				} else
				{
					m = n;
					if (maxStepC <= stepDoneC || !n.RAATillPalindrome(maxStepC - stepDoneC, stepDoneC))
					{
						++lychrelC;
						m_SieveSet.Insert(m);
						if (m_SieveSet.GetSize() != ++sieveSetSize)
							isSetSufficient = false;
					}
				}
			}

			if (!(i & 0x1fff))
			{
				uint32_t tick = ::GetTickCount();
				if (tick - lastTick >= 250)
				{
					lastTick = tick;
					aux::Printf("\r(%u/%u) Testing %s...", MAX_CONSEQ_LEN - len + 1,
						MAX_CONSEQ_LEN - range, SeparateWithCommas(num).c_str());
					if (IsCancelled())
					{
						aux::Print("\n");
						return false;
					}
				}
			}
		}

		::Sleep(1);
		const float elapsed = .000001f * timer.GetElapsed();

		if (len == MAX_CONSEQ_LEN)
		{
			std::string s = util::Format("\rRange: %u-digit, primary Lychrels found: %s\n",
				range, SeparateWithCommas(lychrelC).c_str());
			s.append(s.size() - 2, '-');
			aux::Print(s + "\n");
		}
		aux::Printf("\rLEN=%u, SIFTED=%s (%.2f%%), TIME=%.2fs%s\n", len, SeparateWithCommas(siftedC).c_str(),
			100.f * siftedC / lychrelC, elapsed, isSetSufficient ? "" : " [SET TRUNCATED]");
	}

	return true;
}

// TODO: REF>>>
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Arch::CountLychrels
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
/*Arch::CountLychrels::CountLychrels()
	: m_SieveNumLen(MAX_RANGE + 4)
	, m_PrimLychC(0)
	, m_AllLychC(0)
	, m_AllBaseC(0)
{
}

//----------------------------------------------------------------------------------------------------------------------
void Arch::CountLychrels::Execute(size_t maxRange, bool countBase, const NumberFoundFn& baseLychFn)
{
	maxRange = util::Clamp(maxRange, size_t(1), MAX_RANGE);
	// В качестве длины чисел для набора отсева возьмём длину чисел конечного диапазона проверки, увеличенную
	// на 4-6. Чем выше это значение, тем качественнее и медленнее выполняется отсев. Но у этого алгоритма нет
	// задачи отсеять все числа (т.е. сделать так, чтобы в наборе были только базовые числа Лишрел). Поэтому
	// будет достаточно отсеять большую часть кандидатов, но сделать это быстро
	m_SieveNumLen = maxRange + (countBase ? 6 : 4);

	// Минимальное кол-во операций RAA для каждого диапазона, достаточное для достоверного
	// определения, является ли некоторое его число отложенным палиндром или числом Лишрел
	static const unsigned maxStepA[1 + MAX_RANGE] = { 0, 2, 24, 24, 24, 55, 64, 96,
		96, 98, 109, 149, 149, 188, 188, 201, 201 };

	// Вызовем Clear перед началом работы (подготовим переменные) и после
	// завершения работы (освободим память, занимаемую найденными числами)
	util::FuncToggle cleaner([this](bool) { Clear(); return true; });

	m_Timer.Reset();
	m_AllBase.reserve(2048);
	m_Unsifted.reserve(2048);

	for (size_t curRange = 1; !IsCancelled() && curRange <= maxRange; ++curRange)
	{
		if (!CheckRange(curRange, maxStepA[curRange], countBase))
			break;

		uint64_t rangeSize = 9, checkedC = 9;
		for (size_t i = 1; i < curRange; ++i)
		{
			rangeSize *= 10;
			checkedC += (i & 1) ? checkedC - checkedC / 10 : 9 * checkedC;
		}

		aux::Printf("\rNumbers processed in #2#%2u#7-digit range: #15#%s\n",
			curRange, SeparateWithCommas(checkedC).c_str());
		aux::Printf("#6  Primary lychrel numbers found:     %s #8(%.2f%%)\n",
			SeparateWithCommas(m_PrimLychC).c_str(), 100.f * m_PrimLychC / checkedC);
		aux::Printf("#6  Total amount of lychrel numbers:   %s #8(%.2f%%)\n",
			SeparateWithCommas(m_AllLychC).c_str(), 100.f * m_AllLychC / rangeSize);

		const size_t prevBaseC = m_AllBaseC;
		if (countBase && (m_Unsifted.empty() || CheckUnsifted(baseLychFn)))
		{
			const size_t newC = m_AllBaseC - prevBaseC;
			aux::Printf("#6\r  New base lychrel numbers found:    %s%s #7(%s)\n", newC ? "+" : "",
				SeparateWithCommas(newC).c_str(), SeparateWithCommas(m_AllBaseC).c_str());
		}
	}

	if (!IsCancelled())
		PrintTimeElapsed(true);
}

//----------------------------------------------------------------------------------------------------------------------
void Arch::CountLychrels::Clear()
{
	m_AllBaseC = 0;
	m_SieveSet.Clear(true);
	DeleteNumbers(m_AllBase);
	DeleteNumbers(m_Unsifted);
}

//----------------------------------------------------------------------------------------------------------------------
void Arch::CountLychrels::DeleteNumbers(NumVector& v)
{
	for (FixNumber*& it : v)
		AML_SAFE_DELETEA(it);
	v.clear();
}

//----------------------------------------------------------------------------------------------------------------------
inline FixNumber& Arch::CountLychrels::NextNumber(NumVector& v, size_t& i)
{
	if (i >= GROUP_SIZE)
	{
		i = 0;
		v.push_back(new FixNumber[GROUP_SIZE]);
	}
	return v.back()[i++];
}

//----------------------------------------------------------------------------------------------------------------------
void Arch::CountLychrels::RAATillLength(BigNumber& num, size_t len)
{
	while (num.GetLength() < len)
	{
		unsigned k = static_cast<unsigned>(len - num.GetLength());
		num.ReverseAndAdd((k <= 4) ? k : 2 * k - 4);
	}
}

//----------------------------------------------------------------------------------------------------------------------
bool Arch::CountLychrels::RAATillPalindrome(BigNumber& num, size_t len, unsigned& doneC)
{
	while (num.GetLength() < len)
	{
		unsigned count = 0, k = static_cast<unsigned>(len - num.GetLength());
		bool isPalindrome = num.RAATillPalindrome((k <= 4) ? k : 2 * k - 4, count);
		doneC += count;

		if (isPalindrome)
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool Arch::CountLychrels::CheckRange(size_t range, unsigned maxStepC, bool countBase)
{
	BigNumber num, n, m;
	n.Reserve(2 * m_SieveNumLen);

	char buf[MAX_RANGE + 1] = { '1' };
	memset(buf + 1, '0', range - 1);
	num.Set(buf);

	m_PrimLychC = m_AllLychC = 0;
	size_t unsIdx = GROUP_SIZE;
	DeleteNumbers(m_Unsifted);

	uint32_t lastTick = 0;
	for (size_t i = 0; num.GetLength() == range; ++i, ++num)
	{
		num.SkipRAADups();

		n = num;
		unsigned doneC = 0;
		if (!RAATillPalindrome(n, m_SieveNumLen, doneC))
		{
			bool isPalindrome = false;
			if (!m_SieveSet.Exists(n))
			{
				m = n;
				if (doneC < maxStepC && m.RAATillPalindrome(maxStepC - doneC, doneC))
					isPalindrome = true;
				else
				{
					m_SieveSet.Insert(n);
					if (countBase)
						NextNumber(m_Unsifted, unsIdx) = num;
				}
			}
			if (!isPalindrome)
			{
				++m_PrimLychC;
				m_AllLychC += 1 + num.GetKinNumberC();
			}
		}

		if (!(i & 0x3fff))
		{
			uint32_t tick = ::GetTickCount();
			if (tick - lastTick >= 250)
			{
				lastTick = tick;
				aux::Printf("\rTesting %u-digit number %s...", range,
					SeparateWithCommas(num).c_str());
				if (IsCancelled())
				{
					aux::Print("\n");
					return false;
				}
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool Arch::CountLychrels::CheckUnsifted(const NumberFoundFn& baseLychFn)
{
	// Этот алгоритм аналогичен обычному отсеву с тем отличием, что мы не храним весь набор чисел в памяти (так
	// как их очень много), а разбиваем все числа (новые кандидаты) на несколько групп и проверяем каждую группу
	// отдельно. Второе отличие - глубина RAA, здесь она должна быть достаточно большой, чтобы не "пропустить"
	// вливание кандидата в существующий поток. Для 14-значных чисел значение 16+15=31 оказалось недостаточным,
	// а 16+20=36 - вполне. Также я проверил глубину 16+250=266 (более 500 шагов) для всех чисел до 14-значных
	// включительно и получил тот же результат, что и для глубины 36
	constexpr size_t TARGET_LEN = MAX_RANGE + 35;

	uint32_t lastTick = 0;
	auto progressFn = [&](float progress) {
		uint32_t tick = ::GetTickCount();
		if (tick - lastTick >= 250)
		{
			lastTick = tick;
			aux::Printf("\rChecking out unsifted lychrel numbers: %d%%",
				static_cast<int>(100.f * progress));
			if (IsCancelled())
			{
				aux::Print("\n");
				return false;
			}
		}
		return true;
	};

	BigNumber num;
	std::unordered_map<Number, FixNumber, Number::Hasher> newNumbers;
	for (size_t group = 0; group < m_Unsifted.size();)
	{
		newNumbers.clear();
		// Поместим в набор новые числа. Так как чисел может быть очень много, то
		// за один раз будем брать не более 250 групп (~1M элементов, 100-200 MiB)
		const size_t last = std::min(group + 250, m_Unsifted.size());
		const float progress = static_cast<float>(group) / m_Unsifted.size();
		const float progressEnd = static_cast<float>(last) / m_Unsifted.size();
		for (; group < last; ++group)
		{
			if (!(group & 7) && !progressFn(.7f * progress + .3f * group / m_Unsifted.size()))
				return false;
			const FixNumber* numA = m_Unsifted[group];
			for (size_t i = 0; i < GROUP_SIZE; ++i)
			{
				if (!numA[i])
					break;
				num = numA[i];
				RAATillLength(num, TARGET_LEN);
				newNumbers.emplace(num, numA[i]);
			}
			AML_SAFE_DELETEA(m_Unsifted[group]);
		}
		size_t allIdx = GROUP_SIZE;
		// Проверим наших кандидатов. Если окажется, что для некоторого числа из m_AllBase
		// в newNumbers есть такое же, то удалим соответствующий элемент из newNumbers
		for (size_t i = 0; i < m_AllBase.size(); ++i)
		{
			if (!(i & 7) && !progressFn(progress + (progressEnd - progress) * (.3f + .6f * i / m_AllBase.size())))
				return false;
			const FixNumber* numA = m_AllBase[i];
			for (size_t j = 0; j < GROUP_SIZE; ++j)
			{
				if (!numA[j])
				{
					allIdx = j;
					break;
				}
				num = numA[j];
				RAATillLength(num, TARGET_LEN);
				auto it = newNumbers.find(num);
				if (it != newNumbers.end())
					newNumbers.erase(it);
			}
		}
		// Теперь в newNumbers остались только такие числа, которых нет
		// в m_AllBase. Переместим все числа из newNumbers в m_AllBase
		if (!progressFn(progress + .95f * (progressEnd - progress)))
			return false;
		m_AllBaseC += newNumbers.size();
		for (const auto& it : newNumbers)
			NextNumber(m_AllBase, allIdx) = it.second;

		if (baseLychFn)
		{
			// Пользовательский коллбэк должен вызываться для всех новых чисел. Каждый вызов - следующее число.
			// Числа в группах (и сами группы) расположены по возрастанию, но unordered_map нарушил их порядок
			std::vector<FixNumber> allNew;
			allNew.reserve(newNumbers.size());
			for (const auto& it : newNumbers)
				allNew.push_back(it.second);
			newNumbers.clear();
			std::sort(allNew.begin(), allNew.end());
			for (const auto& n : allNew)
				baseLychFn(num = n);
		}
	}

	m_Unsifted.clear();
	return true;
}*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   A00008 - проверка неразрешимости чисел Лишрел и сохранение результатов проверки в файлы
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Дата: 2019-06-29
// Функция ResolveLychrels выполняет большое количество операций RAA над каждым числом Лишрел в диапазонах чисел
// до максимум 14-значного включительно, чтобы убедиться, что они не дают палиндром (т.е. что они действительно
// являются числами Лишрел). Также эта функция проверяет корректность работы оптимизации отсева чисел Лишрел,
// сходящихся к уже проверенным ранее числам. Ниже перечислены все завершенные тестирования диапазонов:
//		--.--.----: range 1-2,   depth  inf. (no lychrel numbers)
//		28.06.2019: range 3-5,   depth 1000K
//		28.06.2019: range 6-12,  depth 15000
//		28.06.2019: range 13-14, depth  2500

//----------------------------------------------------------------------------------------------------------------------
/*static bool PrintStats(const BigNumber& num, size_t doneC = 0, size_t totalC = 0, unsigned stepC = 0)
{
	static bool showStep = false;
	if (stepC >= 150000) showStep = true;

	static uint32_t lastTick = 0;
	uint32_t tick = ::GetTickCount();

	if (tick - lastTick >= 250)
	{
		lastTick = tick;

		float progress = totalC ? 100.f * doneC / totalC : 0;
		std::string numString = SeparateWithCommas(num);

		if (showStep)
		{
			aux::Printf("\rTesting number #15#%s#7, (%.2f%%): step %s...    \b\b\b\b",
				numString.c_str(), progress, SeparateWithCommas(stepC).c_str());
		} else
		{
			aux::Printf(totalC ? "\rTesting number #15#%s#7, %.2f%% done..." : "\rSkipping number #15#%s#7...",
				numString.c_str(), progress);
		}

		if (util::SystemConsole::Instance().IsCtrlCPressed())
		{
			aux::Print("\n");
			return true;
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool SaveResult(const BigNumber& num, const BigNumber& result, unsigned stepC)
{
	static bool dirCreated = false;
	if (!dirCreated) util::FileSystem::MakeDirectory(L"results");
	dirCreated = true;

	util::BinaryFile f;
	std::wstring filePath = util::Format(L"results/%s.txt", util::FromAnsi(num.AsString()).c_str());
	if (f.Open(filePath, util::FILE_CREATE_ALWAYS | util::FILE_OPEN_WRITE))
	{
		std::string s;
		util::MemoryWriter buf(32);

		s += util::Format("number:%s\n", num.AsString().c_str());
		s += util::Format("iterations:%u\n", stepC);
		s += util::Format("digits:%u\n===\n", result.GetLength());
		buf.Write(s.c_str(), s.size());

		s = result.AsString();
		for (size_t len = 0; len < s.size();)
		{
			size_t toWrite = s.size() - len;
			if (toWrite > 100) toWrite = 100;
			buf.Write(s.c_str() + len, toWrite);
			buf.Write("\n", 1);
			len += toWrite;
		}

		bool ok = buf.SaveTo(f);
		f.Close();
		return ok;
	}
	return false;
}

}  // namespace a00008

//----------------------------------------------------------------------------------------------------------------------
void ResolveLychrels(const BigNumber& startFrom, size_t targetRange, unsigned depth, bool saveResults)
{
	ThreadTime timer;

	const size_t MAX_DIGIT_C = 14;
	// Минимальное кол-во шагов для каждого диапазона, достаточное для достоверного
	// определения, является ли некоторое число отложенным палиндром или числом Лишрел.
	static const unsigned maxStepA[1 + MAX_DIGIT_C] = { 0, 2, 24, 24, 24, 55, 64, 96, 96, 98, 109, 149, 149, 188, 188 };
	// Полное (кумулятивное) значение кол-ва базовых чисел Лишрел в каждом диапазоне.
	static const size_t countA[1 + MAX_DIGIT_C] = { 0, 0, 0, 3, 15, 263, 1202, 15607, 54246, 553260, 1716966,
		15279858, 44444292, 363731086, 1017226026 };

	if (targetRange > MAX_DIGIT_C) targetRange = MAX_DIGIT_C;
	unsigned minDepth = ((maxStepA[targetRange] + 19) / 10) * 10;
	if (depth < minDepth) depth = minDepth;

	BigNumber num(1u), n;
	n.Reserve(MAX_DIGIT_C + depth);

	// Три счетчика: skipC - кол-во базовых чисел, пропущенных перед тестированием (только когда
	// startFrom больше 1); testC - кол-во проверенных чисел Лишрел; kinC - кол-во отсеянных
	// чисел Лишрел, которые сходятся к дургим числам Лишрел, проверенным ранее.
	size_t skipC = 0, testC = 0, kinC = 0;

	bool isCancelled = false;
	if (startFrom.GetLength() <= targetRange)
	{
		// Пропускаем диапазоны до startFrom.
		if (startFrom.GetLength() >= 4)
		{
			char buf[16] = { 0 };
			buf[0] = '1'; for (size_t i = 1; i < startFrom.GetLength(); ++i) buf[i] = '0';
			num.Set(buf); skipC = countA[startFrom.GetLength() - 1];
		}

		using namespace a00008;
		Consequences consequences;
		// Пропускаем числа в диапазоне до startFrom без полной проверки.
		for (size_t it = 0; !isCancelled && (num.GetLength() <= targetRange); ++it, ++num)
		{
			num.SkipRAADups();
			if (num >= startFrom) break;

			n = num;
			unsigned doneC = 0;
			if (!n.RAATillPalindrome(16, doneC))
			{
				minDepth = maxStepA[num.GetLength()];
				if (consequences.IsConsequence(num))
					++kinC;
				else if ((doneC >= minDepth) || !n.RAATillPalindrome(minDepth - doneC, doneC))
				{
					++skipC;
					consequences.AddConsequences(num);
				}
			}

			if ((it & 0x7ff) == 0) isCancelled = PrintStats(num);
		}
		// Очищаем набор отсева, так как непроверенные на полную
		// глубину числа Лишрел мы не можем считать достоверными.
		consequences.Clear();

		// Начальную глубину проверки округляем вверх до 10.
		minDepth = ((maxStepA[num.GetLength()] + 9) / 10) * 10;
		// Главный цикл: проверяем каждое число на заданную глубину.
		for (size_t it = 0, numLength = num.GetLength(); numLength <= targetRange;)
		{
			num.SkipRAADups();

			n = num;
			unsigned totalDoneC = 0, stepDoneC = 0;
			if (!n.RAATillPalindrome(16, totalDoneC))
			{
				if (consequences.IsConsequence(num))
					++kinC;
				else if ((totalDoneC >= minDepth) || !n.RAATillPalindrome(minDepth - totalDoneC, stepDoneC))
				{
					totalDoneC += stepDoneC;
					while (totalDoneC < depth)
					{
						unsigned stepsToPerform = depth - totalDoneC;
						if (n.RAATillPalindrome(std::min(430u, stepsToPerform), stepDoneC))
							throw std::exception("Not a Lychrel number");
						totalDoneC += stepDoneC;

						if ((totalDoneC > 9000) || ((it++ & 0x7f) == 0))
						{
							it = 1;
							isCancelled = PrintStats(num, kinC + testC, countA[targetRange] - skipC, totalDoneC);
							if (isCancelled) break;
						}
					}
					if (isCancelled) break;

					++testC;
					if (saveResults) SaveResult(num, n, totalDoneC);
					// При схеме 12/6+9, мы добавляем числа после 12 шага, а поиск начинаем с 7-го, поэтому
					// некоторые отсеяные числа на самом деле будут "проверены" с глубиной 7 + (depth - 12),
					// т.е. ровно на 5 шагов меньше расчетной глубины поиска. Поэтому, чтобы компенсировать
					// эту разницу, мы просто будем проверять добавляемые числа еще на 5 шагов глубже.
					if (n.RAATillPalindrome(5, stepDoneC)) throw std::exception("Not a Lychrel number");
					consequences.AddConsequences(num);
				}

				if ((it++ & 0x7ff) == 0)
				{
					isCancelled = PrintStats(num, kinC + testC, countA[targetRange] - skipC);
					if (isCancelled) break;
				}
			}

			++num;
			if (num.GetLength() > numLength)
			{
				// Проверим корректность нашего отсева.
				if (skipC + kinC + testC != countA[numLength])
					throw std::exception("Internal error");
				// И рассчитаем новую начальную глубину проверки.
				minDepth = ((maxStepA[++numLength] + 9) / 10) * 10;
			}
		}
	}

	if (!isCancelled)
	{
		if (skipC + testC > 0)
		{
			aux::Printf("\rDone testing all numbers from #15#%s#7 to last #10#%u#7-digit number\n",
				SeparateWithCommas(startFrom).c_str(), targetRange);
		}
		aux::Printf("#15#%s#7 number(s) have been tested with depth #15#%u#7 steps\n",
			SeparateWithCommas(testC).c_str(), depth);
	}

	PrintCPUTime(timer, true);
}*/
