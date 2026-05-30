//∙MDPN
#include "pch.h"
#include "dbase.h"

#include <core/exception.h>
#include <core/filesystem.h>
#include <core/strutil.h>
#include <core/toggle.h>

//----------------------------------------------------------------------------------------------------------------------
DataBase::DataBase()
	: m_Structure(*this)
{
	m_FoundStepA = new bool[Const::MAX_STEP + 1];
	AML_FILLA(m_FoundStepA, 0, Const::MAX_STEP + 1);
	m_PrimLychA = new uint64_t[Const::MAX_DIGIT_C + 1];
	AML_FILLA(m_PrimLychA, 0, Const::MAX_DIGIT_C + 1);
}

//----------------------------------------------------------------------------------------------------------------------
DataBase::~DataBase()
{
	AML_SAFE_DELETEA(m_PrimLychA);
	AML_SAFE_DELETEA(m_FoundStepA);
}

//----------------------------------------------------------------------------------------------------------------------
bool DataBase::Init(bool createNewDb, DBChunkState dataState, const std::wstring& path)
{
	EE::Assert(!m_IsInitialized && !m_IsInitializing, "Can't initialize DB twice");
	util::Toggle<bool> lock(m_IsInitializing, true);

	if (!FindBasePath(path) && !(createNewDb && util::FileSystem::MakeDirectory(m_BasePath, true)))
		return false;

	auto fileList = m_Structure.Reload("Scanning database: %.1f%%...");

	if (fileList.empty() || createNewDb)
		m_IsInitialized = fileList.empty() && createNewDb;
	else
	{
		// TODO: MODE_BACKGROUND (см. ниже) не помогает отвадить Windows Defender от проверки каждого файла
		// при открытии, что жутко тормозит инициализацию БД. Нужен иной способ. Есть идея попробовать
		// многопоточность (это должно ускорить загрузку в случае отсутствия исключений в настройках)

		// TODO: Изменить приоритет на MODE_BACKGROUND очень правильно (особенно для больших БД),
		// но здесь нужна защита от выброшенных исключений (с последующем rethrow), чтобы вернуть
		// обычный класс приоритета процесса и обычный приоритет потока
		//::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
		//::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);

		RearrangeInvalidFiles(fileList, "Rearranging files: %.1f%%...");
		m_Structure.WipeUnusedFolders();

		LoadFileHeaders(fileList, "Loading headers: %1.f%%...");
		fileList.clear();

		dataState = util::Clamp(dataState, DBChunkState::DATAUNLOADED, DBChunkState::WITHSTATS);
		LoadStatistics(dataState, "Loading statistics: %1.f%%...");

		//::SetThreadPriority(::GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
		//::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);

		m_IsInitialized = true;
	}
	return m_IsInitialized;
}

//----------------------------------------------------------------------------------------------------------------------
bool DataBase::SafeInit(const std::wstring& path)
{
	EE::Assert(!m_IsInitialized && !m_IsInitializing, "Can't initialize DB twice");

	m_SafeInitMode = true;
	return Init(false, DBChunkState::DATAUNLOADED, path);
}

//----------------------------------------------------------------------------------------------------------------------
const std::wstring& DataBase::GetBasePath() const
{
	EE::Assert(m_IsInitialized || m_IsInitializing, "Database not initialized");
	return m_BasePath;
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::SetActiveChunk(DBChunk* pChunk)
{
	EE::Assert(m_IsInitialized, "Database not initialized");
	EE::Assert(pChunk && pChunk->GetFirst(), "Uninitialized chunk");

	if (m_pActiveChunk && m_pActiveChunk->GetSaveState() > DBChunkState::UNCHANGED)
	{
		// Сохраняем изменения автоматически, если изменился только заголовок. Если
		// есть изменения в статистике/данных, то требуется сохранение вызовом функции Save
		if (EE::Verify(m_pActiveChunk->GetSaveState() == DBChunkState::HEADERCHANGED, "Not saved changes"))
			Save(0u, 0, 0);
	}

	m_pActiveChunk = pChunk;
	if (!pChunk->HasOwner())
		m_Chunks.Insert(pChunk);

	Number first;
	first = pChunk->GetFirst();
	if ((m_HasGaps && first.GetLength() > m_Last.GetLength()) || (!m_HasGaps && first > m_Last + 1u))
		m_HasGaps = (first - 1u).GetLength() == first.GetLength();
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::AddPalindrome(unsigned step)
{
	EE::Assert(m_pActiveChunk, "No active chunk");
	EE::Assert(step && step <= Const::MAX_STEP, "Invalid step value");

	if (step > m_HighestStep)
		m_HighestStep = step;
	m_FoundStepA[step] = true;

	m_pActiveChunk->AddPalindrome();
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::AddPalindrome(const Number& num, unsigned step)
{
	EE::Assert(m_pActiveChunk, "No active chunk");
	EE::Assert(step && step <= Const::MAX_STEP, "Invalid step value");

	if (step > m_HighestStep)
		m_HighestStep = step;
	m_FoundStepA[step] = true;

	m_pActiveChunk->AddPalindrome(num, step);
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::AddLychrel(const Number& num, unsigned stepC)
{
	const size_t digitC = num.GetLength();
	EE::Assert(m_pActiveChunk, "No active chunk");
	EE::Assert(digitC <= Const::MAX_DIGIT_C, "Invalid Lychrel number");

	++m_PrimLychA[digitC];
	m_pActiveChunk->AddLychrel(num, stepC);
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::Save(const Number& last, unsigned minSavedStep, unsigned timeSpent, bool maxCompression)
{
	EE::Assert(m_pActiveChunk, "No active chunk");

	if (m_pActiveChunk->GetFilePath().empty())
	{
		auto newPath = m_Structure.GetNewFilePath();
		m_pActiveChunk->SetFilePath(newPath);
	}

	if (last)
	{
		m_pActiveChunk->SetLast(last);
		if (last > m_Last)
			m_Last = last;
	}

	if (!m_pActiveChunk->Save(*this, minSavedStep, timeSpent, maxCompression))
	{
		util::FileSystem::RemoveFile(m_BasePath + m_pActiveChunk->GetFilePath());
		throw util::ERuntime("Failed to save database file");
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::ForEachChunk(int& retCode, const std::function<int(DBChunk*)>& fn)
{
	EE::Assert(m_IsInitialized, "Database not initialized");
	retCode = m_Chunks.ForEach(fn);
}

//----------------------------------------------------------------------------------------------------------------------
int DataBase::ForEachChunk(const std::function<int(DBChunk*)>& fn)
{
	EE::Assert(m_IsInitialized, "Database not initialized");
	return m_Chunks.ForEach(fn);
}

//----------------------------------------------------------------------------------------------------------------------
bool DataBase::RemoveChunk(DBChunk* pChunk)
{
	if (pChunk)
	{
		EE::Assert(m_pActiveChunk != pChunk, "Can't remove active chunk");

		const auto filePath = pChunk->GetFilePath();
		m_Chunks.Remove(pChunk);

		if (!filePath.empty() && util::FileSystem::RemoveFile(m_BasePath + filePath))
		{
			m_Structure.OnFileRemoved(filePath);
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool DataBase::FindBasePath(const std::wstring& path)
{
	std::wstring dbPath(util::TrimRight(path)), dbName;
	if (!dbPath.empty())
		dbName = util::FileSystem::ExtractFilename(dbPath);
	if (!dbName.empty())
		dbPath.resize(dbPath.size() - dbName.size());
	else
		dbName = L"data";

	const bool doSearch = dbPath.empty();
	dbPath = util::FileSystem::GetFullPath(dbPath.empty() ? L"./" : dbPath);
	dbName.append(util::FileSystem::DELIMITER);
	if (doSearch)
	{
		for (std::wstring p, next = dbPath; p != next;)
		{
			p = next;
			m_BasePath = p + dbName;
			if (util::FileSystem::DirectoryExists(m_BasePath))
				return true;
			next = util::FileSystem::ExtractPath(util::FileSystem::RemoveTrailingSlashes(p));
		}
	}
	m_BasePath = dbPath + dbName;
	return util::FileSystem::DirectoryExists(m_BasePath);
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::RearrangeInvalidFiles(std::vector<std::wstring>& dbFiles, DBProgress onProgress)
{
	size_t movedC = 0;
	size_t totalDoneC = 0;
	for (auto& filePath : dbFiles)
	{
		++totalDoneC;
		if (DBStructure::IsValidPath(filePath))
			continue;

		if (!(movedC++ & 0x3f) && onProgress)
			onProgress(100.f * (totalDoneC - 1) / dbFiles.size());

		std::wstring newPath = m_Structure.GetNewFilePath();
		if (!util::FileSystem::Rename(m_BasePath + filePath, m_BasePath + newPath))
			throw util::ERuntime("Failed to rename/move database file");
		filePath = std::move(newPath);
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::LoadFileHeaders(std::vector<std::wstring>& dbFiles, DBProgress onProgress)
{
	for (size_t i = 0; i < dbFiles.size(); ++i)
	{
		if (!(i & 0x1f) && onProgress)
			onProgress(100.f * i / dbFiles.size());

		DBChunk* pChunk = new DBChunk;
		m_Chunks.Insert(pChunk);

		// TODO: Сейчас это самая долгая операция во время инициализации БД. Проблема в том, что задержки
		// при последовательном открытии файлов заставляют поток простаивать бОльшую часть времени. Происходит
		// это из-за штатного Windows Defender (другой антивирус должен вести себя аналогично, разве что может
		// работать чуть быстрее), который проверяет каждый новый файл перед открытием (см. подробнее в Init())
		pChunk->SetFilePath(dbFiles[i]);
		if (!pChunk->LoadData(*this, DBChunkState::HEADERONLY))
		{
			if (!m_SafeInitMode)
				throw util::ERuntime("Failed to load database file");
		}
		else if (m_Last < pChunk->GetLast())
			m_Last = pChunk->GetLast();
	}
}

//----------------------------------------------------------------------------------------------------------------------
void DataBase::LoadStatistics(DBChunkState dataState, DBProgress onProgress)
{
	Number first, last;
	size_t processedC = 0;
	unsigned lowestStep = 1;
	const size_t curRange = (m_Last + 1u).GetLength();

	m_Chunks.ForEach([&](DBChunk* pChunk)
	{
		if (!(processedC & 0x3f) && onProgress)
			onProgress(100.f * processedC / m_Chunks.GetSize());
		++processedC;

		// В режиме безопасной загрузки пропустим файл, если мы не смогли загрузить заголовок
		if (m_SafeInitMode && pChunk->GetDataState() < DBChunkState::HEADERONLY)
			return 0;

		first = pChunk->GetFirst();
		Number next = last + 1u;

		if (first < next)
		{
			// Интервал этого файла частично или полностью пересекается с интервалами предыдущих файлов.
			// Такой файл должен быть удалён из БД. Но во время инициализации базы данных мы не вносим
			// в неё изменения, поэтому пока просто пропустим этот файл, оставив его в m_Chunks
		} else
		{
			if (first > next)
			{
				const auto firstLen = first.GetLength();
				// Первое проверенное число этого файла находится на расстоянии от последнего проверенного
				// числа предыдущего файла, образуя "пропущенный" интервал. Такие интервалы проверяются в
				// особом режиме работы программы. Поэтому пока просто установим флаг, если пропущенный
				// интервал лежит в текущем (проверяемом в данный момент) диапазоне чисел БД
				if (firstLen == curRange && (first - 1u).GetLength() == firstLen)
					m_HasGaps = true;
			}

			last = pChunk->GetLast();
			const size_t digitC = last.GetLength();
			m_PrimLychA[digitC] += pChunk->GetPrimaryLychrelC();

			if (!pChunk->LoadData(*this, DBChunkState::WITHSTATS))
			{
				if (!m_SafeInitMode)
				{
					throw util::ERuntime("Failed to load database file");
				}
			} else
			{
				const unsigned* numCountA = pChunk->GetNumCounters();
				const unsigned highestStep = pChunk->GetHighestStep();
				for (unsigned step = lowestStep; step <= highestStep; ++step)
					m_FoundStepA[step] |= numCountA[step] > 0;
				m_HighestStep = std::max(m_HighestStep, highestStep);
				while (lowestStep <= Const::MAX_STEP && m_FoundStepA[lowestStep])
					++lowestStep;
			}
		}
		pChunk->UnloadData(dataState);
		return 0;
	});
}
