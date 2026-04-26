// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "EverythingClient.h"
#include <array>

namespace
{
constexpr DWORD EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME = 0x00000004;
constexpr DWORD EVERYTHING_REQUEST_SIZE = 0x00000010;
constexpr DWORD EVERYTHING_REQUEST_ATTRIBUTES = 0x00000100;

template <typename T>
T GetExport(HMODULE module, const char *name)
{
	return reinterpret_cast<T>(GetProcAddress(module, name));
}

std::wstring QuoteEverythingString(const std::wstring &value)
{
	std::wstring quoted;
	quoted.reserve(value.size() + 2);
	quoted.push_back(L'"');

	for (auto ch : value)
	{
		if (ch == L'"')
		{
			quoted.push_back(L'"');
		}

		quoted.push_back(ch);
	}

	quoted.push_back(L'"');
	return quoted;
}
}

EverythingClient &EverythingClient::GetInstance()
{
	static EverythingClient instance;
	return instance;
}

EverythingClient::EverythingClient() = default;

EverythingClient::~EverythingClient()
{
	if (m_module && m_cleanUp)
	{
		m_cleanUp();
	}

	if (m_module)
	{
		FreeLibrary(m_module);
	}
}

bool EverythingClient::IsAvailable() const
{
	std::lock_guard lock(m_mutex);

	auto *self = const_cast<EverythingClient *>(this);
	return self->Load();
}

std::optional<unsigned long long> EverythingClient::TryGetIndexedSize(const std::wstring &path)
{
	std::lock_guard lock(m_mutex);

	if (!Load())
	{
		return std::nullopt;
	}

	m_setSearchW(QuoteEverythingString(path).c_str());
	m_setMatchCase(FALSE);
	m_setRequestFlags(EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME | EVERYTHING_REQUEST_SIZE);

	if (!m_queryW(TRUE))
	{
		return std::nullopt;
	}

	const auto numResults = m_getNumResults();

	for (DWORD i = 0; i < numResults; i++)
	{
		std::array<wchar_t, MAX_PATH> resultPath;
		m_getResultFullPathNameW(i, resultPath.data(), static_cast<DWORD>(resultPath.size()));

		if (lstrcmpi(resultPath.data(), path.c_str()) != 0)
		{
			continue;
		}

		LARGE_INTEGER size;
		if (!m_getResultSize(i, &size))
		{
			return std::nullopt;
		}

		return static_cast<unsigned long long>(size.QuadPart);
	}

	return std::nullopt;
}

std::optional<std::vector<EverythingSearchResult>> EverythingClient::Query(const std::wstring &query,
	bool matchCase)
{
	std::lock_guard lock(m_mutex);

	if (!Load())
	{
		return std::nullopt;
	}

	m_setSearchW(query.c_str());
	m_setMatchCase(matchCase);
	m_setRequestFlags(EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME | EVERYTHING_REQUEST_SIZE
		| EVERYTHING_REQUEST_ATTRIBUTES);

	if (!m_queryW(TRUE))
	{
		return std::nullopt;
	}

	const auto numResults = m_getNumResults();
	std::vector<EverythingSearchResult> results;
	results.reserve(numResults);

	for (DWORD i = 0; i < numResults; i++)
	{
		std::array<wchar_t, MAX_PATH> resultPath;
		m_getResultFullPathNameW(i, resultPath.data(), static_cast<DWORD>(resultPath.size()));

		LARGE_INTEGER size;
		EverythingSearchResult result;
		result.fullPath = resultPath.data();
		result.attributes = m_getResultAttributes ? m_getResultAttributes(i) : 0;

		if (m_getResultSize(i, &size))
		{
			result.size = static_cast<unsigned long long>(size.QuadPart);
		}

		results.push_back(std::move(result));
	}

	return results;
}

std::optional<std::vector<EverythingIndexedFolderSize>> EverythingClient::QueryIndexedFolderSizes(
	const std::wstring &parentDirectory)
{
	auto results = Query(L"parent:" + QuoteEverythingString(parentDirectory), false);

	if (!results)
	{
		return std::nullopt;
	}

	std::vector<EverythingIndexedFolderSize> folderSizes;

	for (const auto &result : *results)
	{
		DWORD attributes = result.attributes;

		if (attributes == 0)
		{
			attributes = GetFileAttributes(result.fullPath.c_str());
		}

		if (attributes == INVALID_FILE_ATTRIBUTES
			|| (attributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY || !result.size)
		{
			continue;
		}

		folderSizes.push_back({ result.fullPath, *result.size });
	}

	return folderSizes;
}

bool EverythingClient::Load()
{
	if (m_module)
	{
		return true;
	}

	if (m_loadAttempted)
	{
		return false;
	}

	m_loadAttempted = true;

#if defined(_WIN64)
	const wchar_t *dllName = L"Everything64.dll";
#else
	const wchar_t *dllName = L"Everything32.dll";
#endif

	m_module = LoadLibrary(dllName);

	if (!m_module)
	{
		m_module = LoadLibrary(L"Everything.dll");
	}

	if (!m_module)
	{
		return false;
	}

	if (!ResolveExports())
	{
		FreeLibrary(m_module);
		m_module = nullptr;
		ResetExports();
		return false;
	}

	return true;
}

bool EverythingClient::ResolveExports()
{
	m_setSearchW = GetExport<EverythingSetSearchW>(m_module, "Everything_SetSearchW");
	m_setMatchCase = GetExport<EverythingSetMatchCase>(m_module, "Everything_SetMatchCase");
	m_setRequestFlags =
		GetExport<EverythingSetRequestFlags>(m_module, "Everything_SetRequestFlags");
	m_queryW = GetExport<EverythingQueryW>(m_module, "Everything_QueryW");
	m_getNumResults = GetExport<EverythingGetNumResults>(m_module, "Everything_GetNumResults");
	m_getResultFullPathNameW =
		GetExport<EverythingGetResultFullPathNameW>(m_module, "Everything_GetResultFullPathNameW");
	m_getResultSize = GetExport<EverythingGetResultSize>(m_module, "Everything_GetResultSize");
	m_getResultAttributes =
		GetExport<EverythingGetResultAttributes>(m_module, "Everything_GetResultAttributes");
	m_cleanUp = GetExport<EverythingCleanUp>(m_module, "Everything_CleanUp");

	return m_setSearchW && m_setMatchCase && m_setRequestFlags && m_queryW && m_getNumResults
		&& m_getResultFullPathNameW && m_getResultSize;
}

void EverythingClient::ResetExports()
{
	m_setSearchW = nullptr;
	m_setMatchCase = nullptr;
	m_setRequestFlags = nullptr;
	m_queryW = nullptr;
	m_getNumResults = nullptr;
	m_getResultFullPathNameW = nullptr;
	m_getResultSize = nullptr;
	m_getResultAttributes = nullptr;
	m_cleanUp = nullptr;
}
