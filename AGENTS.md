# AGENTS Instructions for AltTab Repository

## Project Overview

AltTab is a Win32 C++ application providing an enhanced alternative to the native Windows Alt+Tab switcher. It adds fuzzy window search, process grouping, window termination, tray integration, and INI-based appearance customization. It targets x64 Windows and is built with CMake, Ninja, and LLVM `clang-cl`.

## Build & Test

### Toolchain

- CMake 3.29 or newer
- Ninja
- LLVM tools: `clang-cl`, `lld-link`, `llvm-rc`, and `llvm-mt`
- 7-Zip for CPack package generation
- Windows SDK headers and libraries
- MSVC-compatible STL and CRT headers and libraries

The Visual Studio IDE, MSBuild, and `cl.exe` are not required. The supported architecture is x64.

If the Windows SDK and MSVC-compatible STL/CRT are installed through Visual Studio or Build Tools, initialize an x64 environment with `vcvars64.bat` before configuring. This only exposes SDK/CRT paths; CMake invokes LLVM `clang-cl` and Ninja.

### Presets

- `clang-debug` - Debug build
- `clang-release` - Release build with logging disabled
- `clang-release-logger` - Release build with logging enabled

Configure and build:

```powershell
cmake --preset clang-release
cmake --build --preset clang-release --parallel
```

Create a CPack archive with:

```powershell
cmake --build --preset clang-release --target package
```

Build artifacts are written below `out/`. The optional `AltTabTester` target is enabled with `-DALTTAB_BUILD_TESTER=ON`. Logging is controlled by `ALTTAB_ENABLE_LOGGER`.

### Code Style & Formatting

- **Formatter**: `clang-format` configured in `.clang-format`
- **Format**: `clang-format -i <file>`
- Key settings: 4-space indent, 120-character column limit, left pointer alignment, and intentional include ordering.

## Architecture & Key Components

### Core Application Flow

1. **Entry Point** (`AltTab.cpp`) - Initializes global state, creates the main window, and sets up the keyboard hook
2. **Main Window** (`AltTabWindow.cpp/h`) - Renders the window UI and list view
3. **Keyboard Hook** (`LLKeyboardProc`) - Captures Alt+Tab, Alt+Backtick, and Alt+Ctrl+Tab
4. **Settings** (`AltTabSettings.cpp/h`) - Loads, saves, and validates INI configuration

### Key Files by Responsibility

| File | Purpose |
| --- | --- |
| `AltTab.cpp/h` | Global state, keyboard hook, tray icon, and elevation handling |
| `AltTabWindow.cpp/h` | Window rendering, list view, mouse, and keyboard handling |
| `AltTabSettings.cpp/h` | INI parsing, serialization, defaults, and validation |
| `Logger.cpp/h` | Optional log4cpp wrapper |
| `CheckForUpdates.cpp/h` | Version checking |
| `fuzzywuzzy.cpp/h` | Fuzzy string matching |
| `Utils.cpp/h` | Win32 process and window utilities |
| `GlobalData.cpp/h` | Global settings instance and access patterns |
| `Tooltips.h` | Custom tooltip structures |

### Configuration System

- **Settings file**: `AltTabSettings.ini` in the application directory
- **Sections**: `[SearchString]`, `[ListView]`, `[General]`, `[Hotkeys]`, `[Backtick]`, and `[ProcessExclusions]`
- **Colors**: hexadecimal RGB values such as `0xFF0000`
- **Font styles**: `normal`, `italic`, `bold`, or `bold italic`
- **Reload**: tray menu -> `Reload AltTabSettings.ini`

### Window Enumeration & Process Groups

- `SimilarProcessGroups` groups related executables, using `process1.exe/process2.exe|process3.exe/process4.exe` syntax.
- Alt+Backtick switches within the current group; Alt+Tab switches across all windows.
- `ProcessExclusions` hides specified processes.

### Global State Variables

```cpp
HWND g_hAltTabWnd;
HWND g_hFGWnd;
HWND g_hMainWnd;
HWND g_hSettingsWnd;
HHOOK g_KeyboardHook;
AltTabSettings g_Settings;
```

### Settings Access Pattern

```cpp
extern AltTabSettings g_Settings;
```

### Window Messages

```cpp
UINT const WM_USER_ALTTAB_TRAYICON = WM_APP + 1;
```

### Logging

Logging is optional and enabled only when the CMake `ALTTAB_ENABLE_LOGGER` option is enabled. Usage follows the existing `LOG(INFO) << "message"` pattern.

## Common Tasks

### Adding a New Settings Option

1. Add the field and default value to `AltTabSettings.h`.
2. Parse it in `AltTabSettings.cpp`.
3. Serialize it in the save path.
4. Extend `IsValid()`.
5. Add a Settings dialog control in `AltTab.rc` when needed.

### Modifying Window List Filtering

Main filtering is in `AltTabWindow::GetWindowList()` and related enumeration functions. Fuzzy matching is in `fuzzywuzzy.cpp`; process grouping is parsed from settings.

### Handling Keyboard Shortcuts

The low-level hook handles Alt+Tab, Alt+Shift+Tab, Alt+Backtick, Alt+Shift+Backtick, Alt+Ctrl+Tab, Delete, and Shift+Delete according to the settings flags.

### Running as Administrator

Elevation helpers live in `Utils.cpp`. Elevated AltTab may be required to control elevated applications.

## Known Limitations

- Hotkeys do not work for an elevated foreground application unless AltTab also runs elevated.
- Multiple-monitor support is not implemented.
- Some highlight colors are configurable only through the INI file.

## Resource Files

- `AltTab.rc` - Dialogs, strings, icons, and version information
- `resource.h` - Resource identifiers
- `AltTabHelp.hnd`, `Help.mht`, and `Help.docx` - User documentation

## Version Tracking

- **Format**: `MAJOR.MINOR.PATCH.BUILD`
- **Input**: `project(... VERSION ...)` in `CMakeLists.txt`
- **Generated output**: CMake generates `version.h` in the build tree
