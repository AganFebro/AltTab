# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working in this repository.

## Project Overview

AltTab is a Win32 C++ desktop application that replaces the native Windows Alt+Tab task switcher. It provides fuzzy-matched window search, Alt+Backtick switching within related applications, process grouping, process termination, tray integration, and INI-based appearance customization. It targets x64 Windows and is built with CMake, Ninja, and LLVM `clang-cl`.

## Build

Required tooling:

- CMake 3.29 or newer
- Ninja
- LLVM `clang-cl`, `lld-link`, `llvm-rc`, and `llvm-mt`
- 7-Zip for CPack package generation
- Windows SDK headers and libraries
- MSVC-compatible STL and CRT headers and libraries

The Visual Studio IDE, MSBuild, and `cl.exe` are not required.

If the Windows SDK and MSVC-compatible STL/CRT are installed through Visual Studio or Build Tools, initialize an x64 environment with `vcvars64.bat` before configuring. This only exposes SDK/CRT paths; CMake invokes LLVM `clang-cl` and Ninja.

Supported presets:

- `clang-debug` - Debug build
- `clang-release` - Release build with logging disabled
- `clang-release-logger` - Release build with logging enabled

```powershell
cmake --preset clang-release
cmake --build --preset clang-release --parallel
```

To package the release:

```powershell
cmake --build --preset clang-release --target package
```

Build artifacts are placed below `out/`. The optional `AltTabTester` target is enabled with `-DALTTAB_BUILD_TESTER=ON`. Logging is controlled by `ALTTAB_ENABLE_LOGGER`.

## Formatting

`clang-format` is configured in `.clang-format`. Apply it with `clang-format -i <file>`. Style is 4-space indentation, a 120-column limit, left pointer alignment, and intentional include ordering.

## Architecture

Single-process GUI app. Startup flow (`source/AltTab.cpp`): initialize global state -> create a hidden main message window -> install a global low-level keyboard hook -> add the tray icon.

- **`AltTab.cpp/h`** - Entry point, global state, keyboard hook, tray icon, and elevation handling. Custom message: `WM_USER_ALTTAB_TRAYICON = WM_APP + 1`.
- **`AltTabWindow.cpp/h`** - Visible switcher window, list view, rendering, context menu, and tooltips.
- **`AltTabSettings.cpp/h`** - `AltTabSettings`, INI load/save, defaults, and validation.
- **`GlobalData.cpp/h`** - Global settings instance (`g_Settings`) and access patterns.
- **`fuzzywuzzy.cpp/h`** - Fuzzy string matching for window filtering.
- **`Utils.cpp/h`** - Win32 process, window, and elevation utilities.
- **`Logger.cpp/h`** - Optional log4cpp wrapper using `LOG(INFO) << ...`.
- **`AltTab.rc` / `resource.h`** - Dialogs, strings, icons, and version resources.

## Settings / INI

`AltTabSettings.ini` lives beside the application. Sections include `[SearchString]`, `[ListView]`, `[General]`, `[Hotkeys]`, `[Backtick]`, and `[ProcessExclusions]`. Colors use hexadecimal RGB such as `0xRRGGBB`. The file can be edited manually and reloaded through the tray menu.

- **`SimilarProcessGroups`** groups related executables with `p1.exe/p2.exe|p3.exe/p4.exe` syntax. Alt+Backtick cycles within a group; Alt+Tab spans all windows.
- **`ProcessExclusions`** hides specified executables.

To add a setting, add its field and default in `AltTabSettings.h`, parse and serialize it in `AltTabSettings.cpp`, extend `IsValid()`, and add a Settings dialog control in `AltTab.rc` when needed.

## Conventions

- Strings use `wchar_t`, `std::wstring`, and wide Win32 APIs.
- Global variables use the `g_` prefix.
- Windows handles use `h`/`m_h` prefixes; members use `m_`.

## Versioning and Packaging

`project(... VERSION ...)` in `CMakeLists.txt` is the version source in `MAJOR.MINOR.PATCH.BUILD` format. CMake generates `version.h` in the build tree and CPack creates release archives. The old Visual Studio/Python build and packaging helpers are removed.

## Known Limitations

- Hotkeys do not reach an elevated foreground app unless AltTab itself runs elevated.
- Multi-monitor support is not implemented.
