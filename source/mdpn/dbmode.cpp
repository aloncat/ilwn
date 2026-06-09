//∙MDPN
// Copyright (C) 2019-2026 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "dbmode.h"

#include <core/console.h>
#include <core/filesystem.h>
#include <core/strformat.h>

//--------------------------------------------------------------------------------------------------------------------------------
size_t DBMode::GetDataSize(const DBChunk* chunk, size_t realSize)
{
	static size_t valueIdx = 1;
	// 0-й элемент - усреднённое значение остальных элементов
	static float valueA[1 + 4] = { 7.65f, 7.7f, 7.7f, 7.6f, 7.6f };
	constexpr size_t valueC = sizeof(valueA) / sizeof(valueA[0]) - 1;

	if (realSize)
	{
		const auto& numbers = chunk->GetNumbers();
		const size_t count = std::max(size_t(1), numbers.size());
		valueA[valueIdx] = static_cast<float>(realSize) / count;
		valueIdx = 1 + (valueIdx) % valueC;

		float sum = 0;
		for (int i = 1; i <= valueC; ++i)
			sum += valueA[i];
		valueA[0] = sum / valueC;

		return realSize;
	}

	const size_t numberC = chunk->GetNumbers().size();
	return static_cast<size_t>(numberC * valueA[0]);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring DBMode::TruncateDatabasePath(const std::wstring& path, size_t lengthLimit)
{
	std::wstring out = util::FileSystem::RemoveTrailingSlashes(path);
	lengthLimit = std::max(size_t(10), lengthLimit);

	if (out.size() > lengthLimit)
	{
		size_t toRemove = out.size() - lengthLimit + 3;
		size_t nextSlash = out.find_first_of(L"/\\", 3);
		while (nextSlash != std::wstring::npos && nextSlash - 3 < toRemove)
			nextSlash = out.find_first_of(L"/\\", nextSlash + 1);
		out.replace(3, (nextSlash != std::wstring::npos) ? nextSlash - 3 : toRemove, L"...");
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DBMode::PrintDatabasePath(const std::wstring& path, size_t lengthLimit)
{
	auto dbPath = TruncateDatabasePath(path, lengthLimit);
	util::SystemConsole::Instance().SetTitle(util::Format(L"{%s} - MDPN", dbPath.c_str()));

	if (util::StrNCmp(path, dbPath, dbPath.size()))
		dbPath.replace(3, 3, L"#8...#7#");

	EventManager::PublishEvent(L"#6Path:#7 " + dbPath);
}
