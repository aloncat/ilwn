//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "searchbase.h"

#include "powers.h"
#include "util.h"

#include <auxlib/print.h>
#include <core/strformat.h>
#include <core/strutil.h>
#include <core/sysinfo.h>
#include <core/winapi.h>

//--------------------------------------------------------------------------------------------------------------------------------
SearchBase::~SearchBase()
{
	m_Log.Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchBase::Run(int power, int leftCount, int rightCount)
{
	if (power < 1 || power > MAX_POWER || leftCount < 1 || rightCount < 2 ||
		leftCount >= MAX_FACTOR_COUNT || rightCount >= MAX_FACTOR_COUNT ||
		leftCount > rightCount || leftCount + rightCount > MAX_FACTOR_COUNT)
	{
		aux::Printc("#12Error: equation parameters are incorrect\n");
		return false;
	}

	m_Info.power = power;
	m_Info.leftCount = leftCount;
	m_Info.rightCount = rightCount;

	m_StartTick = ::GetTickCount();
	m_RunningTime = 0;

	if (!InitOptions())
		return false;

	// Извлекаем из командной строки стартовые значения первых коэффициентов.
	// Если ничего не было задано, то функция венёт вектор с одним элементом
	const auto startFactors = GetStartFactors();

	// Вектор будет пустым, если произошла ошибка. Если задано
	// слишком много коэффициентов, то также выдаём сообщение об ошибке
	if (startFactors.empty() || startFactors.size() >= m_Info.leftCount + m_Info.rightCount)
	{
		aux::Printc("#12Error: equation coefficients are incorrect\n");
		return false;
	}

	UpdateConsoleTitle();
	aux::Printf("Searching for factors of equation #6#%i.%i.%i#7, #2[Z]#7 starts from #10#%u\n",
		m_Info.power, m_Info.leftCount, m_Info.rightCount, startFactors[0]);

	const auto maxFactorCount = std::max(m_Info.leftCount, m_Info.rightCount);
	// NB: в процессе перебора решений алгоритм вычисляет суммы левой и правой частей по отдельности. Мы должны
	// быть уверены, что ни для каких значений коэффициентов, сумма их степеней не вызовет переполнения. Поэтому
	// при нахождении макс. значения старшего коэффициента используем maxFactorCount в качестве множителя
	const unsigned upperLimit = Powers<UInt128>::CalcUpperValue(m_Info.power, maxFactorCount);
	aux::Printf("Factor limit is set to #10#%s #8(128 bits)\n", SeparateWithCommas(upperLimit).c_str());

	if (startFactors[0] > upperLimit)
	{
		aux::Print("Noting to do, exiting...\n");
		return false;
	}

	if (!OpenLogFile())
	{
		aux::Printc("#12Error: failed to open log file\n");
		return false;
	}

	if (m_Info.leftCount == 1 && m_Info.rightCount == 2 && m_Info.power > 2)
	{
		// Для уравнений вида p.1.2 при p > 2 просто предупреждаем
		aux::Printc("#12WARNING: #3this equation has no solutions!\n");
	}

	if (auto info = GetAdditionalInfo(); !info.empty())
		aux::Printf(L"#10Using#7 %s\n", info.c_str());

	Search(startFactors);
	m_Log.Close();

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchBase::PrintOptionsHelp()
{
	aux::Printc("#15Options:\n"
		"#3  --printall     #7Print all found solutions (first 1K otherwise)\n"
		"#3  --thread <N>   #7Set initial count of active threads to N\n"
	);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchBase::UpdateRunningTime()
{
	const auto secondsElapsed = (::GetTickCount() - m_StartTick) / 1000;
	m_StartTick += 1000 * secondsElapsed;
	m_RunningTime += secondsElapsed;
}

//--------------------------------------------------------------------------------------------------------------------------------
float SearchBase::GetRunningTime() const
{
	return m_RunningTime + .001f * (::GetTickCount() - m_StartTick);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring SearchBase::GetAdditionalInfo() const
{
	return std::wstring();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchBase::OpenLogFile()
{
	if (m_Log.Open(util::Format(L"log-%i.%i.%i.txt", m_Info.power, m_Info.leftCount, m_Info.rightCount),
		util::FILE_OPEN_ALWAYS | util::FILE_OPEN_READWRITE))
	{
		if (auto fileSize = m_Log.GetSize(); fileSize > 0)
		{
			if (m_Log.SetPosition(fileSize) && m_Log.Write("---\n", 4))
				return true;
		}
		else if (!fileSize)
			return true;

		m_Log.Close();
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::vector<unsigned> SearchBase::GetStartFactors()
{
	std::vector<unsigned> factors;

	// NB: первые 3 параметра - это всегда параметры уравнения. Далее опционально следуют стартовые
	// коэффициенты (любое количество, меньшее суммарного количества коэффициентов). Затем следуют
	// опции, начинающиеся на "--". После первой опции не должно быть ничего, кроме опций
	auto& params = util::SystemInfo::Instance().GetCommandLineParameters();

	for (size_t i = 3; i < params.size(); ++i)
	{
		if (auto s = util::Trim(params[i]); !s.empty())
		{
			if (!util::StrNCmp(s, L"--", 2))
				break;

			if (!IsPositiveInteger(s) || s.size() > 10)
				return {};

			factors.push_back(wcstoul(s.c_str(), nullptr, 10));
		}
	}

	// Проверяем набор коэффициентов: они должны быть
	// ненулевыми, а последовательность - неубывающей
	for (size_t i = 0; i < factors.size(); ++i)
	{
		if (!factors[i] || (i && factors[i - 1] < factors[i]))
			return {};
	}

	if (factors.empty())
	{
		// Если из командной строки коэффициенты извелечь не
		// удалось, то мы начнём с наименьшего возможного
		factors.push_back(2);
	}

	return factors;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SearchBase::InitOptions()
{
	auto& params = util::SystemInfo::Instance().GetCommandLineParameters();

	bool hasOptions = false;
	for (size_t i = 3; i < params.size(); ++i)
	{
		if (auto s = util::Trim(params[i]); !s.empty())
		{
			// Опции должны начинаться с "--"
			if (util::StrNCmp(s, L"--", 2))
			{
				if (!hasOptions)
					continue;

				// Некорректный параметр в командной строке
				aux::Printc(util::Formatter<wchar_t>() << L"#12Error: "
					"invalid token \"" << s << L"\"\n");
				return false;
			}
				
			hasOptions = true;
			std::wstring_view key = s;
			key.remove_prefix(2);
	
			if (!key.empty())
			{
				// Опция "--printall", разрешает вывод большого количества решений на экран
				if (!util::StrInsCmp(key, L"printall"))
				{
					m_Options.AddOption(key);
					continue;
				}

				// Опция "--thread <N>", задаёт количество активных потоков
				if (!util::StrInsCmp(key, L"thread"))
				{
					if (i + 1 < params.size() && IsPositiveInteger(params[i + 1]))
					{
						m_Options.AddOption(key, params[++i]);
						continue;
					}
				}

				// Опция не распознана или некорректна
				aux::Printc(util::Formatter<wchar_t>() << L"#12Error: "
					"invalid option \"" << s << L"\"\n");
				return false;
			}
		}
	}

	return true;
}
