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

	m_FactorCount = m_Info.leftCount + m_Info.rightCount;
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
				return [](const WorkerTask& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 3);
				};
			}

			// Уравнения 4.1.n (n < 16) и 8.1.n (n < 32): Z не может быть чётным для n < 16 (4.1.n) или
			// n < 32 (8.1.n); Z не может быть кратным 3 для n < 3; Z не может быть кратным 5 для n < 5
			if ((m_Info.power == 4 || m_Info.power == 8) && m_Info.rightCount < 5)
			{
				if (m_Info.rightCount < 3)
				{
					return [](const WorkerTask& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 5);
					};
				}

				return [](const WorkerTask& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 5);
				};
			}

			// Уравнение 6.1.n (n < 8): Z не может быть чётным для n < 8; Z не может
			// быть кратным 3 для n < 9; Z не может быть кратным 7 для n < 7
			if (m_Info.power == 6 && m_Info.rightCount < 9)
			{
				if (m_Info.rightCount < 7)
				{
					return [](const WorkerTask& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3) && (task.factors[0] % 7);
					};
				}
				if (pow2Cond)
				{
					return [](const WorkerTask& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 3);
					};
				}

				return [](const WorkerTask& task) {
					return (task.factors[0] % 3) != 0;
				};
			}

			// Уравнение 10.1.n (n < 8): Z не может быть чётным
			// для n < 8; Z не может быть кратным 11 для n < 11
			if (m_Info.power == 10 && m_Info.rightCount < 11)
			{
				if (pow2Cond)
				{
					return [](const WorkerTask& task) {
						return (task.factors[0] & 1) && (task.factors[0] % 11);
					};
				}

				return [](const WorkerTask& task) {
					return (task.factors[0] % 11) != 0;
				};
			}

			// Уравнение 12.1.n (n < 16): Z не может быть чётным
			// для n < 16; Z не может быть кратным 13 для n < 13
			if (m_Info.power == 12 && m_Info.rightCount < 13)
			{
				return [](const WorkerTask& task) {
					return (task.factors[0] & 1) && (task.factors[0] % 13);
				};
			}

			if (pow2Cond)
			{
				// Чётные степени: Z не может быть чётным
				return [](const WorkerTask& task) {
					return (task.factors[0] & 1) != 0;
				};
			}
		}
		// Для p.2.n при чётных p коэффициенты левой части не могут быть чётными одновременно
		else if (m_Info.leftCount == 2 && pow2Cond)
		{
			return [](const WorkerTask& task) {
				return (task.factors[0] & 1) || (task.factors[1] & 1);
			};
		}
	}

	// Для других случаев оптимизаций нет
	return [](const WorkerTask&) { return true; };
}

//--------------------------------------------------------------------------------------------------------------------------------
void SearchAny::InitFirstTask(WorkerTask& task, const std::vector<unsigned>& startFactors)
{
	// Для универсального алгоритма все коэффициенты
	// левой части уравнения всегда заданы заданием
	task.factorCount = m_Info.leftCount;

	// TODO: это нужно доработать -- для случаев более 1 заданного коэффициента правой части (актуально
	// для больших степеней и большого количества коэффициентов в правой части); также стоит рассмотреть
	// обратную ситуацию, когда мы бы хотели задавать заданием не все коэффициенты левой части
	if (m_Info.power >= 5 && m_Info.leftCount == 1)
	{
		const int p = m_Info.power;
		const int l = m_Info.leftCount;
		const int c = m_Info.rightCount - l;
		// Дополнительно задаваемые коэффициенты правой части
		const float predefined = .09f * (p + 2) + .13f * (c + 1) - .5f * l;

		static const int maxCount[11] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 };
		const int maxPredefined = (m_Info.power <= 10) ? maxCount[m_Info.power] : 1;
		task.factorCount += util::Clamp(static_cast<int>(predefined), 0, maxPredefined);
	}

	const int count = std::min(task.factorCount, static_cast<int>(startFactors.size()));

	for (size_t i = 0; i < count; ++i)
		task.factors[i] = startFactors[i];

	for (int i = count; i < task.factorCount; ++i)
		task.factors[i] = 1;

	// При количестве перебираемых коэффициентов равном 2 или 3 заменим функцию поиска
	if (int freeFactors = m_FactorCount - task.factorCount; freeFactors < 4)
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
void SearchAny::SelectNextTask(WorkerTask& task)
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
	using Fn = void (SearchAny::*)(WorkerTask&, const NumberT*) const;
	auto fn = static_cast<Fn>(&SearchAny::template SelectNext);
	return reinterpret_cast<SelectNextFn>(fn);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
void SearchAny::SelectNext(WorkerTask& task, const NumberT* powers) const
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

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE void SearchAny::SearchFactors(Worker* worker, const NumberT* powers)
{
	// Коэффициенты уравнения
	unsigned factors[MAX_FACTOR_COUNT];

	const WorkerTask& task = *worker->task;
	// Копируем коэффициенты из задания
	for (int i = 0; i < task.factorCount; ++i)
		factors[i] = task.factors[i];

	// Количество "перебираемых" коэффициентов
	const int count = m_FactorCount - task.factorCount;
	// Массив k, начиная с k[1], содержит перебираемые коэффициенты
	unsigned* k = factors + task.factorCount - 1;

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	// Отнимаем значение степеней всех заданных коэф-в правой части
	for (int i = m_Info.leftCount; i < task.factorCount; ++i)
		z -= powers[factors[i]];

	k[1] = 1;
	// Пропускаем низкие значения k[1], не дающие решений
	for (unsigned step = factors[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * count)
			k[1] = f;
	}

	// Предельное значение для k[1] (старшего из перебираемых коэффициентов)
	const unsigned high = (task.factorCount > m_Info.leftCount) ? k[0] + 1 :
		(m_Info.leftCount == 1 || m_Info.leftCount == m_Info.rightCount) ? factors[0] : UINT_MAX;

	// Перебор старшего коэффициента
	for (int rem = count - 1; k[1] < high; ++k[1])
	{
		const auto pk1 = powers[k[1]];
		if (pk1 + rem > z)
			break;

		auto zd = z - pk1;
		// Перебор других коэффициентов
		for (int idx = (count > 4) ? 2 : 0;;)
		{
			if (idx)
			{
				// Значение idx будет равно 1, если мы достигли предела для k[2]. Это возможно,
				// если count > 4. В этом случае закончим внутренний цикл, чтобы увеличить k[1]
				if (idx == 1)
					break;

				// Каждый раз, когда мы достигаем предела значения перебираемого коэффициента, мы увеличиваем
				// на 1 коэффициент слева от него, а в переменной idx сохраняем индекс самого левого коэф-та,
				// который необходимо сбросить. Единичное и многие следующие значения коэффициента не смогут
				// дать решений, так как для них сумма в правой части будет меньше значения левой. Поэтому
				// мы будем пропускать такие значения, сразу переходя к тем, которые могут дать решение
				for (unsigned hi = k[idx - 1]; rem > 3; --rem)
				{
					k[idx] = 1;
					for (unsigned step = hi >> 1; step; step >>= 1)
					{
						auto f = k[idx] + step;
						if (zd > powers[f - 1] * rem)
							k[idx] = f;
					}
					hi = k[idx++];
					zd -= powers[hi];
				}

				idx = 0;
			}

			// Отдельно перебираем младшие 3 коэффициента.
			// Функция вернёт true, если поиск был прерван
			if (SearchLast(worker, zd, k + count - 3, powers))
				return;

			// Если у нас 4 перебираемых коэффициента (для этой функции меньше быть не может),
			// то старший из них перебирается во внешнем цикле, поэтому делать ничего не нужно
			if (count <= 4)
				break;

			// Выбираем следующий набор коэффициентов. Инкрементируем, начиная с последнего. В
			// случае достижения коэффициентом максимального значения, перейдём к следующему и
			// сохраним в idx его индекс (чтобы сбросить все такие коэф-ты в следующем цикле)
			for (int i = count - 3;;)
			{
				zd += powers[k[i]];
				if (k[i - 1] > k[i])
				{
					const auto f = ++k[i];
					if (auto pk = powers[f]; pk + rem <= zd)
					{
						zd -= pk;
						break;
					}
				}

				++rem;
				idx = i--;
				if (i == 1)
				{
					idx = 1;
					break;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class NumberT>
AML_NOINLINE bool SearchAny::SearchLast(Worker* worker, NumberT z, unsigned* k, const NumberT* powers)
{
	// На входе z содержит разность суммы коэффициентов левой части и старших коэффициентов правой
	// части до k[0] включительно. k[0], k[1], k[2] и k[3] - это младшие коэффициенты правой части
	const unsigned* factors = k + 4 - m_FactorCount;

	k[1] = 1;
	// Пропустим низкие значения k[1]
	for (unsigned step = k[0] >> 1; step; step >>= 1)
	{
		auto f = k[1] + step;
		if (z > powers[f - 1] * 3)
			k[1] = f;
	}

	// Перебор значений коэффициента k[1]
	for (size_t it = 0; k[1] <= k[0]; ++k[1])
	{
		const auto pk1 = powers[k[1]];
		if (pk1 + 2 > z)
			break;

		k[2] = 1;
		const auto zd = z - pk1;
		// Пропустим низкие значения k[2]
		for (unsigned step = k[1] >> 1; step; step >>= 1)
		{
			auto f = k[2] + step;
			if (zd > powers[f - 1] << 1)
				k[2] = f;
		}

		// Перебор значений k[2]
		for (; k[2] <= k[1]; ++k[2])
		{
			const auto pk2 = powers[k[2]];
			if (pk2 >= zd)
				break;

			// Вычисляем значение степени последнего коэффициента правой части, при котором может существовать
			// решение уравнения, и проверяем значение в хеш-таблице. Если значение не будет найдено, то значит
			// не существует такого целого числа, степень которого равна lastFP и можно пропустить этот набор
			if (const auto lastFP = zd - pk2; m_Hashes.Exists(lastFP))
			{
				// Хеш был обнаружен. Теперь нужно найти число, соответствующее значению степени lastFP. Так
				// как массив степеней powers упорядочен по возрастанию, то используем бинарный поиск. Если
				// коллизии хешей не было, то мы обнаружим значение в массиве, а его индекс будет искомым
				// числом. Искомое значение не может превышать значения предпоследнего коэффициента
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
							return true;
						break;
					}
				}
			}

			// Периодически выводим текущий прогресс. Если пользователь
			// нажмёт Ctrl-C, то функция вернёт true, мы завершим работу
			if (!(it++ & 0xfff) && !(++worker->progressCounter & 0x7f))
			{
				if (OnProgress(worker, factors))
					return true;
			}
		}
	}

	return false;
}

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
	factors[0] = worker->task->factors[0];
	factors[1] = worker->task->factors[1];

	// Значение левой части
	auto z = powers[factors[0]];
	if (m_Info.leftCount >= 2)
		z += powers[factors[1]];

	// Массив k, начиная с индекса 1, содержит коэффициенты правой части.
	// В элементе k[0] хранится предшествующий им коэффициент левой части
	unsigned* k = factors + worker->task->factorCount - 1;

	k[1] = 1;
	// Пропускаем низкие значения старшего коэффициента
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

	const WorkerTask& task = *worker->task;
	// Копируем коэффициенты из задания
	for (int i = 0; i < task.factorCount; ++i)
		factors[i] = task.factors[i];

	// Значение левой части
	auto z = powers[factors[0]];
	for (int i = 1; i < m_Info.leftCount; ++i)
		z += powers[factors[i]];

	// Массив k, начиная с индекса 1, содержит коэффициенты правой части.
	// В элементе k[0] хранится предшествующий им коэффициент левой части
	unsigned* k = factors + task.factorCount - 1;

	// Если в левой части 2 коэффициента (в правой всегда 3), то значение k[1]
	// (старшего коэффициента правой части) ограничено только значением суммы
	const unsigned high = (m_Info.leftCount != 2) ? factors[0] : UINT_MAX;

	k[1] = 1;
	// Пропускаем низкие значения старшего коэффициента
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
