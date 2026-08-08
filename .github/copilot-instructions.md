# Copilot Instructions for AltTab Repository

## Project Overview

AltTab is a Win32 C++ application providing an enhanced alternative to the native Windows Alt+Tab switcher. It supports fuzzy window search, process grouping, process termination, tray integration, and INI-based appearance customization. It targets x64 Windows and is built with CMake, Ninja, and LLVM `clang-cl`.

## Build & Test

Required tooling:

- CMake 3.29 or newer
- Ninja
- LLVM `clang-cl`, `lld-link`, `llvm-rc`, and `llvm-mt`
- 7-Zip for CPack package generation
- Windows SDK headers and libraries
- MSVC-compatible STL and CRT headers and libraries

The Visual Studio IDE, MSBuild, and `cl.exe` are not required. Use the x64 CMake presets:

If the Windows SDK and MSVC-compatible STL/CRT are installed through Visual Studio or Build Tools, initialize an x64 environment with `vcvars64.bat` before configuring. This only exposes SDK/CRT paths; CMake invokes LLVM `clang-cl` and Ninja.

```powershell
cmake --preset clang-debug
cmake --build --preset clang-debug --parallel

cmake --preset clang-release
cmake --build --preset clang-release --parallel

cmake --preset clang-release-logger
cmake --build --preset clang-release-logger --parallel
```

`clang-release` disables logging; `clang-release-logger` enables it. Logging is controlled by `ALTTAB_ENABLE_LOGGER`. Build artifacts are written below `out/`.

Package a configured release build with:

```powershell
cmake --build --preset clang-release --target package
```

The optional `AltTabTester` target is enabled with `-DALTTAB_BUILD_TESTER=ON`.

## Code Style

Use the existing `.clang-format` configuration with `clang-format -i <file>`. Preserve intentional include ordering. The code uses Unicode wide strings and wide Win32 APIs.

## Architecture & Key Components

1. **Entry Point** (`AltTab.cpp`) - Initializes state, creates the hidden main window, and installs the keyboard hook.
2. **Main Window** (`AltTabWindow.cpp/h`) - Renders the switcher and manages the list view.
3. **Keyboard Hook** (`LLKeyboardProc`) - Handles Alt+Tab, Alt+Backtick, Alt+Ctrl+Tab, and termination shortcuts.
4. **Settings** (`AltTabSettings.cpp/h`) - Loads, validates, and saves `AltTabSettings.ini`.

| File | Purpose |
| --- | --- |
| `AltTab.cpp/h` | Global state, keyboard hook, tray icon, and elevation handling |
| `AltTabWindow.cpp/h` | Rendering, list view, mouse, keyboard, and context menu handling |
| `AltTabSettings.cpp/h` | Settings structure, defaults, parsing, serialization, and validation |
| `Logger.cpp/h` | Optional log4cpp wrapper |
| `CheckForUpdates.cpp/h` | Version checking |
| `fuzzywuzzy.cpp/h` | Fuzzy matching |
| `Utils.cpp/h` | Win32 process and window utilities |
| `GlobalData.cpp/h` | Global settings access |
| `AltTab.rc` / `resource.h` | Dialogs, strings, icons, and version resources |

## Settings

`AltTabSettings.ini` is beside the executable. Sections include `[SearchString]`, `[ListView]`, `[General]`, `[Hotkeys]`, `[Backtick]`, and `[ProcessExclusions]`. Colors are hexadecimal RGB values such as `0xFF0000`. `SimilarProcessGroups` uses `p1.exe/p2.exe|p3.exe/p4.exe` syntax, and `ProcessExclusions` hides selected executables.

When adding a setting, update the field/default, load path, save path, and `IsValid()`. Add a control in `AltTab.rc` if it belongs in the Settings dialog.

## Versioning and Packaging

`project(... VERSION ...)` in `CMakeLists.txt` is the `MAJOR.MINOR.PATCH.BUILD` input. CMake generates `version.h` in the build tree; CPack creates release archives. Obsolete Visual Studio and Python build/package helpers are no longer used.

## Known Limitations

- Hotkeys do not reach an elevated foreground application unless AltTab also runs elevated.
- Multiple-monitor support is not implemented.
- Some highlight colors are INI-only.
