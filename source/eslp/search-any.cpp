//∙ESLP/iLWN
// Copyright (C) 2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "search-any.h"

#include "progressman.h"
#include "uint128.h"

#include <core/debug.h>

//--------------------------------------------------------------------------------------------------------------------------------
void SearchAny::BeforeCompute(unsigned upperLimit)
{
	FactorSearch::BeforeCompute(upperLimit);
	SetSearchFn(this);

	m_CheckTaskFn = GetCheckTaskFn();

	m_SelectNextFn = nullptr;
	if (m_Powers->IsType<uint64_t>())
		m_SelectNextFn = GetSelectNextFn<uint64_t>();
	else if (m_Powers->IsType<UInt128>())
		m_SelectNextFn = GetSelectNextFn<UInt128>();
	Assert(m_SelectNextFn);
}

//--------------------------------------------------------------------------------------------------------------------------------
FactorSearch::CheckTaskFn SearchAny::GetCheckTaskFn() const
{
	if (!(m_Info.power & 1))
	{
		// Переменная pow2Cond будет иметь значение true, если актуальна оптимизация кратности 2, то есть если
		// количество коэффициентов в правой части меньше модуля, по которому степень числа сравнима с единицей
		const bool pow2Cond = (m_Info.power == 2 && m_Info.rightCount < 4) || (m_Info.power >= 4 && m_Info.power <= 20 &&
			(m_Info.rightCount < 8 || (!(m_Info.power & 3) && m_Info.rightCount < 16) || (!(m_Info.power & 7) &&
			m_Info.rightCount < 32) || (m_Info.power == 16 && m_Info.rightCount < 64)));

		if (m_Info.leftCount == 1)
		{
			// Уравнение 2.1.n (n < 4): Z не может быть чётным
			// для n < 4; Z не может быть кратным 3 для n < 3
			if (m_Info.power == 2 && m_Info.rightCount < 3)
			{
				return [](const Task& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 3);
				};
			}

			// Уравнения 4.1.n (n < 16) и 8.1.n (n < 32): Z не может быть чётным для n < 16 (4.1.n) или
			// n < 32 (8.1.n); Z не может быть кратным 3 для n < 3; Z не может быть кратным 5 для n < 5
			if ((m_Info.power == 4 || m_Info.power == 8) && m_Info.rightCount < 5)
			{
				if (m_Info.rightCount < 3)
				{
					return [](const Task& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 5);
					};
				}

				return [](const Task& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 5);
				};
			}

			// Уравнение 6.1.n (n < 8): Z не может быть чётным для n < 8; Z не может
			// быть кратным 3 для n < 9; Z не может быть кратным 7 для n < 7
			if (m_Info.power == 6 && m_Info.rightCount < 9)
			{
				if (m_Info.rightCount < 7)
				{
					return [](const Task& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 7);
					};
				}
				if (pow2Cond)
				{
					return [](const Task& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3);
					};
				}

				return [](const Task& task) -> bool {
					return task.factors[0] % 3;
				};
			}

			// Уравнение 10.1.n (n < 8): Z не может быть чётным
			// для n < 8; Z не может быть кратным 11 для n < 11
			if (m_Info.power == 10 && m_Info.rightCount < 11)
			{
				if (pow2Cond)
				{
					return [](const Task& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 11);
					};
				}

				return [](const Task& task) -> bool {
					return task.factors[0] % 11;
				};
			}

			// Уравнение 12.1.n (n < 16): Z не может быть чётным
			// для n < 16; Z не может быть кратным 13 для n < 13
			if (m_Info.power == 12 && m_Info.rightCount < 13)
			{
				return [](const Task& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 13);
				};
			}

			if (pow2Cond)
			{
				// Чётные степени: Z не может быть чётным
				return [](const Task& task) -> bool {
					return task.factors[0] & 1;
				};
			}
		}
		// Для p.2.n при чётных p коэффициенты левой части не могут быть чётными одновременно
		else if (m_Info.leftCount == 2 && pow2Cond)
		{
			return [](const Task& task) {
				return (task.factors[0] & 1) || (task.factors[1] & 1);
			};
		}
	}

	// Для других случаев оптимизаций нет
	return [](const Task&) { return true; };
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchAny::InitFirstTask(Task& task, const std::vector<unsigned>& startFactors)
{
	// Для универсального алгоритма все коэффициенты
	// левой части уравнения всегда заданы заданием
	task.factorCount = m_Info.leftCount;

	// TODO: это нужно доработать (-2..+3 коэф-та)
	if (m_Info.power > 5 && m_Info.leftCount == 1)
	{
		const int p = m_Info.power;
		const int c = m_Info.rightCount - m_Info.leftCount;
		// Дополнительно задаваемые коэффициенты правой части
		const float predefined = .08f * (p + 2) + .2f * (c + 1) - .4f;

		static const int maxCount[11] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 };
		const int maxPredefined = (m_Info.power <= 10) ? maxCount[m_Info.power] : 1;
		task.factorCount += util::Clamp(static_cast<int>(predefined), 0, maxPredefined);
	}

	const int count = std::min(task.factorCount, static_cast<int>(startFactors.size()));

	for (size_t i = 0; i < count; ++i)
		task.factors[i] = startFactors[i];

	for (int i = count; i < task.factorCount; ++i)
		task.factors[i] = 1;

	// При количестве перебираемых коэффициентов равном 2 или 3 заменим функцию поиска
	if (int freeFactors = m_Info.leftCount + m_Info.rightCount - task.factorCount; freeFactors < 4)
	{
		// Функции поиска для 2 и 3 свободных коэффициентов полагают, что
		// эти коэффициенты полностью формируют правую часть уравнения
		Assert(freeFactors > 1 && freeFactors == m_Info.rightCount);

		m_SearchFn = nullptr;
		if (m_Powers->IsType<uint64_t>())
			m_SearchFn = GetSearchFn<uint64_t>(freeFactors);
		else if (m_Powers->IsType<UInt128>())
			m_SearchFn = GetSearchFn<UInt128>(freeFactors);
		Assert(m_SearchFn);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchAny::SelectNextTask(Task& task)
{
	// Перебираем только коэф-ты левой части
	if (task.factorCount <= m_Info.leftCount)
	{
		unsigned* k = task.factors;
		for (int i = task.factorCount - 1; i; --i)
		{
			if (k[i - 1] > k[i])
			{
				++k[i];
				return;
			}

			k[i] = 1;
		}

		++k[0];
		return;
	}

	// Перебираем коэффициенты левой и правой частей
	(this->*m_SelectNextFn)(task, m_Powers->GetData());
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
SearchAny::SelectNextFn SearchAny::GetSelectNextFn()
{
	using Fn = void (SearchAny::*)(Task&, const NumberT*) const;
	auto fn = static_cast<Fn>(&SearchAny::template SelectNext);
	return reinterpret_cast<SelectNextFn>(fn);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void SearchAny::SelectNext(Task& task, const NumberT* powers) const
{
	unsigned* k = task.factors;
	const int m = m_Info.leftCount;

	// Сумма в левой части
	auto z = powers[k[0]];
	for (int i = 1; i < m; ++i)
		z += powers[k[i]];

	// Отнимаем n-1 заданных коэффициентов правой
	for (int i = m; i < task.factorCount - 1; ++i)
		z -= powers[k[i]];

	int idx = 0;
	bool ready = false;

	// Выбираем следующий набор коэф-в правой части
	int rem = m + m_Info.rightCount - task.factorCount;
	for (int i = task.factorCount - 1; i > m; --i, ++rem)
	{
		if (k[i - 1] > k[i])
		{
			auto f = ++k[i];
			if (z >= powers[f] + rem)
			{
				ready = true;
				break;
			}
		}

		idx = i;
		k[i] = 1;

		z += powers[k[i - 1]];
	}

	if (!ready)
	{
		auto f = ++k[m];
		// Если в левой и правой частях одинаковое количество коэф-в, то значение
		// старшего коэф-та правой части должно быть строго меньше значения k[0]
		if ((m != m_Info.rightCount || f < k[0]) && z >= powers[f] + m)
			ready = true;
		else
		{
			idx = m;
			k[m] = 1;
		}
	}

	// Левая часть
	while (!ready)
	{
		int i;
		for (i = m - 1; i; --i)
		{
			if (k[i - 1] > k[i])
				break;
			k[i] = 1;
		}

		++k[i];
		ready = m_CheckTaskFn(task);
	}

	if (idx)
	{
		// Если idx != 0, то мы сбросили коэффициенты в 1, начиная от idx,
		// и поэтому теперь нужно пропустить низкие значения этих коэф-в
		for (int i = idx; i < task.factorCount; ++i)
		{
			unsigned hi = (i == m) ? k[0] : k[i - 1];
			for (unsigned step = hi >> 1; step; step >>= 1)
			{
				auto f = k[i] + step;
				if (z > powers[f - 1] * rem)
					k[i] = f;
			}

			// Если hi не является степенью двойки, то k[i] может быть немного
			// меньше, чем необходимо. Поэтому попробуем уточнить его значение
			for (; z > powers[k[i]] * rem; ++k[i]);

			z -= powers[k[i]];
			--rem;
		}
	}
}

////--->

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchAny::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Коэффициенты уравнения
	unsigned factors[MAX_FACTOR_COUNT];

	// Копируем коэффициенты из задания
	for (int i = 0; i < worker->task.factorCount; ++i)
		factors[i] = worker->task.factors[i];

	// Коэффициенты, не заданные заданием, инициализируем в 1
	const int factorCount = m_Info.leftCount + m_Info.rightCount;
	for (int i = worker->task.factorCount; i < factorCount; ++i)
		factors[i] = 1;

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	NumberT sum = 0;
	// Сумма правой части без последнего коэффициента
	for (int i = m_Info.leftCount; i < factorCount - 1; ++i)
		sum += powers[factors[i]];

	// Количество "перебираемых" коэффициентов
	const int count = factorCount - worker->task.factorCount;
	// Массив k, начиная с индекса 1, содержит перебираемые коэффициенты правой части уравнения.
	// В элементе k[0] хранится предшествующий им коэффициент левой или правой части уравнения
	unsigned* k = factors + worker->task.factorCount - 1;

	sum -= count - 1;
	// Пропустим часть значений 1-го перебираемого коэффициента
	// правой части, при которых набор не будет давать решение
	for (; z > sum + powers[k[1]] * count; ++k[1]);
	sum += powers[k[1]] + count - 2;

	if (sum >= z)
		return;

	// Перебираем коэффициенты правой части
	for (size_t it = 0;;)
	{
		// Вычисляем значение степени последнего коэффициента правой части, при котором может существовать
		// решение уравнения, и проверяем значение в хеш-таблице. Если значение не будет найдено, то значит
		// не существует такого целого числа, степень которого равна lastFP и можно пропустить этот набор
		if (const auto lastFP = z - sum; m_Hashes.Exists(lastFP))
		{
			// Хеш был обнаружен. Теперь нужно найти число, соответствующее значению степени lastFP. Так
			// как массив степеней powers упорядочен по возрастанию, то используем бинарный поиск. Если
			// коллизии хешей не было, то мы обнаружим значение в массиве, а его индекс будет искомым
			// числом. Искомое значение не может превышать значения предпоследнего коэффициента
			for (unsigned lo = 1, hi = k[count - 1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[count] = mid;
					if (OnSolutionFound(worker, factors))
						return;
					break;
				}
			}
		}

		// Периодически выводим текущий прогресс. Если пользователь
		// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
		if (!(it++ & 0x7ff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, factors))
				return;
		}

		// Увеличиваем предпоследний коэффициент в правой части
		if (auto f = k[count - 1]; f < k[count - 2])
		{
			if (auto n = sum - powers[f] + powers[f + 1]; n < z)
			{
				k[count - 1] = f + 1;
				sum = n;
				continue;
			}
		}

		int idx = 0;
		// Значение предпоследнего коэффициента или сумма оказались слишком большими. Переходим
		// к следующему набору коэффициентов правой части, меняя сразу несколько коэффициентов
		for (int i = count - 1;; --i)
		{
			sum -= powers[k[i]];
			if (k[i - 1] > k[i] || i == 1)
			{
				const auto f = ++k[i];
				if (auto n = sum + powers[f]; n < z)
				{
					sum = n;
					break;
				}
				else if (i == 1)
					return;
			}
			sum += k[i] = 1;
			idx = i;
		}

		if (idx)
		{
			if (idx == 2)
			{
				// Для уравнений с одинаковым числом коэффициентов в левой и правой частях мы не хотим перебирать
				// старшие коэффициенты правой части, значения которых >= значению старшего коэффициента левой части
				if (k[1] >= factors[0] && m_Info.leftCount == m_Info.rightCount)
					return;

				// Если в правой части уравнения хотя бы 1 коэффициент задан
				// заданием, то значение k[1] не должно превышать значения k[0]
				if (k[1] > k[0] && worker->task.factorCount > m_Info.leftCount)
					return;
			}

			// Каждый раз, когда мы сбрасываем в 1 коэффициент в правой части, увеличивая на 1 коэффициент слева от него,
			// переменная idx будет содержать индекс самого левого единичного коэффициента. Единичное и многие следующие
			// значения коэффициента не смогут дать решений, так как для них сумма в правой части будет меньше значения
			// левой. Поэтому мы будем пропускать такие значения, сразу переходя к тем, которые могут дать решение
			unsigned hi = k[idx - 1];
			for (int rem = count - idx + 1; rem > 1; --rem)
			{
				const auto s = sum - (rem - 1);
				for (unsigned step = hi >> 1; step; step >>= 1)
				{
					auto f = k[idx] + step;
					if (z > s + powers[f - 1] * rem)
						k[idx] = f;
				}

				hi = k[idx++];
				sum += powers[hi] - 1;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchAny::SearchLast(NumberT z, unsigned* k, const NumberT* powers)
{
	// TODO
}

////<---

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
FactorSearch::SearchFn SearchAny::GetSearchFn(int freeFactors)
{
	Assert(freeFactors == 2 || freeFactors == 3);

	using Fn = void (SearchAny::*)(Worker*, const NumberT*);
	Fn fn = (freeFactors == 2) ? static_cast<Fn>(&SearchAny::template SearchFactors2) :
		static_cast<Fn>(&SearchAny::template SearchFactors3);

	return reinterpret_cast<SearchFn>(fn);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchAny::SearchFactors2(Worker* worker, const NumberT* powers)
{
	// Коэффициенты уравнения: 1-2 в левой части, 2 в правой
	static_assert(ProgressManager::MAX_COEFS >= 4);
	unsigned factors[ProgressManager::MAX_COEFS];

	// Копируем коэффициенты из задания
	for (int i = 0; i < worker->task.factorCount; ++i)
		factors[i] = worker->task.factors[i];

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	// Массив k, начиная с индекса 1, содержит перебираемые коэффициенты правой части уравнения.
	// В элементе k[0] хранится предшествующий им коэффициент левой или правой части уравнения
	unsigned* k = factors + worker->task.factorCount - 1;

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = factors[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] << 1)
			k[1] = f;
	}

	for (; k[1] < factors[0]; ++k[1])
	{
		if (const auto lastFP = z - powers[k[1]]; m_Hashes.Exists(lastFP))
		{
			for (unsigned lo = 1, hi = k[1]; lo <= hi;)
			{
				unsigned mid = (lo + hi) >> 1;
				if (auto v = powers[mid]; v < lastFP)
					lo = mid + 1;
				else if (v > lastFP)
					hi = mid - 1;
				else
				{
					k[2] = mid;
					if (OnSolutionFound(worker, factors))
						return;
					break;
				}
			}
		}
	}

	// Вывод прогресса
	if (!(++worker->progressCounter & 0xff))
		OnProgress(worker, factors);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchAny::SearchFactors3(Worker* worker, const NumberT* powers)
{
	// Коэффициенты уравнения: 1-3 в левой части, 3 в правой
	static_assert(ProgressManager::MAX_COEFS >= 6);
	unsigned factors[ProgressManager::MAX_COEFS];

	// Копируем коэффициенты из задания
	for (int i = 0; i < worker->task.factorCount; ++i)
		factors[i] = worker->task.factors[i];

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	// Массив k, начиная с индекса 1, содержит перебираемые коэффициенты правой части уравнения.
	// В элементе k[0] хранится предшествующий им коэффициент левой или правой части уравнения
	unsigned* k = factors + worker->task.factorCount - 1;

	// Если в левой части 2 коэффициента (в правой всегда 3), то значение k[1]
	// (старшего коэффициента правой части) ограничено только значением суммы
	const unsigned high = (m_Info.leftCount != 2) ? factors[0] : UINT_MAX;

	k[1] = 1;
	// Пропускаем низкие значения старшего коэф-та
	for (unsigned step = factors[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * 3)
			k[1] = f;
	}

	// Перебор старшего коэф-та правой части
	for (size_t it = 0; k[1] < high; ++k[1])
	{
		const auto pk1 = powers[k[1]];
		if (pk1 + 2 > z)
			break;

		k[2] = 1;
		const auto zd = z - pk1;
		// Пропускаем низкие значения 2-го коэф-та
		for (unsigned step = k[1] >> 1; step; step >>= 1)
		{
			auto f = k[2] + step;
			if (zd > powers[f - 1] << 1)
				k[2] = f;
		}

		// Перебор 2-го коэф-та
		for (; k[2] <= k[1]; ++k[2])
		{
			const auto pk2 = powers[k[2]];
			if (pk2 >= zd)
				break;

			// Поиск 3-го коэффициента
			if (const auto lastFP = zd - pk2; m_Hashes.Exists(lastFP))
			{
				for (unsigned lo = 1, hi = k[2]; lo <= hi;)
				{
					unsigned mid = (lo + hi) >> 1;
					if (auto v = powers[mid]; v < lastFP)
						lo = mid + 1;
					else if (v > lastFP)
						hi = mid - 1;
					else
					{
						k[3] = mid;
						if (OnSolutionFound(worker, factors))
							return;
						break;
					}
				}
			}
		}

		// Вывод прогресса
		if (!(it++ & 0xff) && !(++worker->progressCounter & 0x7f))
		{
			if (OnProgress(worker, factors))
				return;
		}
	}
}
