// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#pragma once

#include <optional>
#include <string>
#include <mutex>
#include <vector>

struct EverythingSearchResult
{
	std::wstring fullPath;
	DWORD attributes = 0;
	std::optional<unsigned long long> size;
};

struct EverythingIndexedFolderSize
{
	std::wstring fullPath;
	unsigned long long size;
};

class EverythingClient
{
public:
	static EverythingClient &GetInstance();

	bool IsAvailable() const;
	std::optional<unsigned long long> TryGetIndexedSize(const std::wstring &path);
	std::optional<std::vector<EverythingSearchResult>> Query(const std::wstring &query,
		bool matchCase);
	std::optional<std::vector<EverythingIndexedFolderSize>> QueryIndexedFolderSizes(
		const std::wstring &parentDirectory);

private:
	EverythingClient();
	~EverythingClient();

	EverythingClient(const EverythingClient &) = delete;
	EverythingClient &operator=(const EverythingClient &) = delete;

	bool Load();
	bool ResolveExports();
	void ResetExports();

	HMODULE m_module = nullptr;
	bool m_loadAttempted = false;
	mutable std::mutex m_mutex;

	using EverythingSetSearchW = void(__stdcall *)(LPCWSTR lpSearchString);
	using EverythingSetMatchCase = void(__stdcall *)(BOOL bEnable);
	using EverythingSetRequestFlags = void(__stdcall *)(DWORD dwRequestFlags);
	using EverythingQueryW = BOOL(__stdcall *)(BOOL bWait);
	using EverythingGetNumResults = DWORD(__stdcall *)();
	using EverythingGetResultFullPathNameW = DWORD(__stdcall *)(DWORD nIndex, LPWSTR lpString,
		DWORD nMaxCount);
	using EverythingGetResultSize = BOOL(__stdcall *)(DWORD nIndex, LARGE_INTEGER *lpFileSize);
	using EverythingGetResultAttributes = DWORD(__stdcall *)(DWORD nIndex);
	using EverythingCleanUp = void(__stdcall *)();

	EverythingSetSearchW m_setSearchW = nullptr;
	EverythingSetMatchCase m_setMatchCase = nullptr;
	EverythingSetRequestFlags m_setRequestFlags = nullptr;
	EverythingQueryW m_queryW = nullptr;
	EverythingGetNumResults m_getNumResults = nullptr;
	EverythingGetResultFullPathNameW m_getResultFullPathNameW = nullptr;
	EverythingGetResultSize m_getResultSize = nullptr;
	EverythingGetResultAttributes m_getResultAttributes = nullptr;
	EverythingCleanUp m_cleanUp = nullptr;
};
