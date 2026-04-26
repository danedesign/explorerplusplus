# Agent Notes for Explorer++ Everything Fork

This file is project memory for future coding agents. Keep it updated when changing the
Everything integration, folder-size behavior, build setup, or packaging flow. Do not assume the
repository remains at the same absolute path; all commands below should be run from the repository
root after `cd` into wherever the folder currently lives.

## User Goal

The user is maintaining a fork of Explorer++ as a possible primary Windows file manager. The main
goals are:

- Integrate Everything search so Explorer++ search feels closer to Windows File Explorer's address
  bar search experience.
- Add a top search box beside the address/toolbar area with two scopes: current folder and whole
  drive.
- Use Everything's existing index/database for search and folder sizes; do not make Explorer++
  build a separate index.
- Make folder sizes in Details view appear quickly, ideally immediately, by using Everything's
  indexed folder-size data instead of recursively walking folders one by one.
- Keep Chinese UI font rendering modern and system-consistent. Do not hard-code Microsoft YaHei;
  prefer the user's system UI font.
- Keep the app portable: a release folder should be usable as a standalone app folder.

## Important Everything Details

- The Everything SDK DLL is a client IPC DLL only. Copying `Everything64.dll` beside
  `Explorer++.exe` does not duplicate the Everything index and does not re-scan disks by itself.
  It talks to the user's already running/installed Everything service/app.
- For x64 builds, the portable release folder needs `Everything64.dll` next to `Explorer++.exe`.
- If Everything is unavailable, the code should fall back to Explorer++'s old behavior rather than
  failing hard.
- Fast folder sizes depend on Everything having folder size indexing enabled in Everything's own
  options. If Everything has no indexed folder size for a folder, Explorer++ may still fall back to
  recursive calculation and appear slow.

## Current Implementation Map

- `Explorer++/Helper/EverythingClient.h`
- `Explorer++/Helper/EverythingClient.cpp`
  - Dynamically loads `Everything64.dll`, `Everything32.dll`, or `Everything.dll`.
  - Wraps Everything SDK calls under a mutex.
  - Provides single-path size lookup, general search query, and batch child-folder size lookup.
  - Calls `Everything_CleanUp` and `FreeLibrary` safely.

- `Explorer++/Helper/FolderSize.cpp`
  - `GetFolderInfo()` first asks `EverythingClient::TryGetIndexedSize(path)`.
  - Falls back to old recursive folder walking if Everything cannot provide a size.

- `Explorer++/Explorer++/SearchDialog.cpp`
- `Explorer++/Explorer++/SearchDialog.h`
  - Search tries Everything first.
  - Falls back to original recursive search when needed.
  - Contains fixes for queued PIDL cleanup, search-result timer shutdown, invalid regex thread
    reference handling, CreateThread failure handling, and the old fallback search skipping the
    first item.

- `Explorer++/Explorer++/MainRebar.cpp`
- `Explorer++/Explorer++/Explorer++.h`
  - Adds a top search control with current-folder and whole-drive scope.
  - Search bar text must not be hard-coded to Chinese. It should use resource strings:
    `IDS_SEARCH_SCOPE_CURRENT_FOLDER`, `IDS_SEARCH_SCOPE_WHOLE_DRIVE`, and `IDS_TOOLBAR_SEARCH`,
    with safe English fallback if a translation DLL does not contain the newer strings.

- `Explorer++/Explorer++/ShellBrowser/BrowsingHandler.cpp`
- `Explorer++/Explorer++/ShellBrowser/ColumnManager.cpp`
- `Explorer++/Explorer++/ShellBrowser/ListView.cpp`
- `Explorer++/Explorer++/ShellBrowser/ShellBrowserImpl.cpp`
- `Explorer++/Explorer++/ShellBrowser/ShellBrowserImpl.h`
  - Details-view folder size performance path.
  - On folder insertion and when switching to Details view, `PrimeFolderSizeCacheFromEverything()`
    asks Everything once for all direct child folder sizes.
  - Folder-size values are stored in `m_directoryState.cachedFolderSizes`.
  - `MaybeGetCachedColumnText()` serves the Size column immediately from cache and avoids queuing
    the old per-item background column calculation when possible.
  - Honors the existing setting that disables folder sizes on network/removable drives.

- `Explorer++/Explorer++/BaseDialog.cpp`
- `Explorer++/Explorer++/BaseDialog.h`
- `Explorer++/Explorer++/Explorer++.rc`
  - Chinese font rendering was improved by using the system UI font/runtime dialog font behavior
    instead of forcing an old-looking dialog font.

- `Explorer++/Helper/Helper.vcxproj`
- `Explorer++/Helper/Helper.vcxproj.filters`
  - Include the new `EverythingClient` source/header in the Helper project.

## Reference File Note

The user pointed to `reference/BetterFileSizesInExplorerDetails.cpp` as a Windhawk reference for
native Explorer folder-size behavior. The inspected file was not actually a folder-size
implementation; it appeared to be a Windhawk Paint sample/template. Do not rely on it as source
logic unless the reference file is replaced with the real mod.

## Build Notes

The user has Visual Studio installed. A known working MSBuild path on this machine during the
session was:

```powershell
C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe
```

Build the main app project directly, not the full solution, unless installer work is needed. The
full solution may try to build the WiX installer and fail if WiX is not installed.

Preferred local build command from repository root:

```powershell
.\build-release-x64.ps1
```

From repository root:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "Explorer++\Explorer++\Explorer++.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:SolutionDir="$PWD\Explorer++\"
```

If `$PWD` expansion causes issues in Windows PowerShell, use:

```powershell
$repo = (Get-Location).Path
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" "Explorer++\Explorer++\Explorer++.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:SolutionDir="$repo\Explorer++\"
```

Previous Codex sandbox builds failed before C++ compilation with:

```text
MSB3491: Could not write lines to file "...Helper.lastbuildstate". Access to the path is denied.
```

That error was a sandbox/intermediate-file write issue, not evidence of a C++ compile error.

Syntax-only checks that passed in the prior session:

- `EverythingClient.cpp`
- `FolderSize.cpp`
- `SearchDialog.cpp`
- `MainRebar.cpp`
- `BaseDialog.cpp`
- `ShellBrowser/BrowsingHandler.cpp`
- `ShellBrowser/ColumnManager.cpp`
- `ShellBrowser/ListView.cpp`
- `ShellBrowser/ShellBrowserImpl.cpp`

## Packaging Notes

- Portable folder previously used:
  `dist/Explorer++-Everything-x64`
- Keep in the portable release folder:
  - `Explorer++.exe`
  - `Everything64.dll`
  - any runtime/config/assets that Explorer++ actually needs
- Do not include build scripts/intermediate files in the end-user portable folder.

## Testing Checklist

After building:

1. Put `Everything64.dll` next to the newly built `Explorer++.exe`.
2. Start or keep Everything running.
3. Confirm Everything folder-size indexing is enabled.
4. Launch Explorer++ from the portable folder.
5. Test top search box:
   - current folder scope
   - whole drive scope
   - file names that are known to exist
6. Test Details view folder sizes:
   - open a folder with many subfolders
   - ensure the Size column appears quickly instead of slowly filling row by row
   - test switching from another view into Details view
7. Test fallback behavior:
   - temporarily remove/rename `Everything64.dll`
   - confirm Explorer++ still runs and search/folder-size behavior falls back rather than crashing
8. Test Chinese UI:
   - toolbar/search controls
   - dialogs
   - menus and labels
9. Watch for memory/leak regressions around:
   - closing SearchDialog while results are queued
   - cancelling search
   - invalid regex search
   - repeated folder navigation in Details view

## Development Rules for Future Agents

- Preserve user changes. Do not reset or revert unrelated files.
- Use repo-relative paths and avoid hard-coded absolute paths.
- Prefer small, focused changes in the existing Explorer++ style.
- Keep Everything optional and dynamically loaded.
- Keep folder-size code fast but non-blocking where possible; if adding slower work, avoid doing it
  on the UI thread.
- After any code, documentation, build, or packaging change, update this `AGENTS.md` file when the
  change affects future context, workflow, testing, or user preferences.
- After any change, provide the user with a concise commit message they can use directly.
- If build validation is blocked by the environment, run syntax-only checks and clearly say what was
  and was not verified.
