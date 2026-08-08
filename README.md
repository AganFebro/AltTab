![GitHub Downloads][gh-downloads]

# AltTab
AltTab is a lightweight native Win32 alternative to the Windows task switcher. It adds instant fuzzy search,
process grouping, window/process actions, and a modern Windows 11-style interface without a UI framework.

## Build

AltTab is built on Windows with CMake, Ninja, and the LLVM `clang-cl` toolchain. The supported target is x64.

Prerequisites:

* CMake 3.29 or newer
* Ninja
* LLVM/Clang with `clang-cl`, `lld-link`, `llvm-rc`, and `llvm-mt`
* Windows SDK headers and libraries
* MSVC-compatible STL and CRT headers and libraries
* 7-Zip (required by CPack's 7Z generator)

The Visual Studio IDE, MSBuild, and the Microsoft C++ compiler are not required. The Windows SDK and MSVC-compatible STL/CRT are needed only for their headers and libraries.

If those SDK/CRT files are installed through Visual Studio or Build Tools, initialize an x64 developer environment with its `vcvars64.bat` before configuring. This only exposes the SDK/CRT paths; CMake still invokes LLVM `clang-cl` and Ninja.

Configure and build a release package:

```powershell
cmake --preset clang-release
cmake --build --preset clang-release --parallel
```

Other supported presets are `clang-debug` and `clang-release-logger`. Logging is optional and is enabled by the latter preset. To build the optional tester target, configure with `-DALTTAB_BUILD_TESTER=ON`:

```powershell
cmake --preset clang-debug -DALTTAB_BUILD_TESTER=ON
cmake --build --preset clang-debug --target AltTabTester --parallel
```

Create a CPack package after building:

```powershell
cmake --build --preset clang-release --target package
```

Run the focused theme and settings-migration checks with:

```powershell
ctest --preset clang-release --output-on-failure
```

Build output is kept under `out/`. Do not use the removed `.sln`, `.vcxproj`, or MSBuild packaging scripts.

## Usage

Run `AltTab.exe` once. The application stays in the notification area and opens the switcher with the configured hotkeys:

* `Alt+Tab` / `Alt+Shift+Tab`: switch between windows.
* `Alt+Backtick` / `Alt+Shift+Backtick`: switch within a similar-process group.
* `Alt+Ctrl+Tab`: show the switcher without immediately changing windows.
* Type a title or process name to filter the list.
* `Delete` closes normally; `Shift+Delete` force-terminates the selected window/process.
* `Escape` cancels the switcher.

`AltTabSettings.ini` is stored beside the executable. It controls appearance, hotkeys, process groups, and process
exclusions. Reload it from the notification-area menu after manual edits.

## Appearance

The Settings dialog offers four theme modes:

* `System` (default) follows the Windows app light/dark setting.
* `Light` and `Dark` force the built-in modern palettes.
* `Custom` uses the legacy INI font, color, highlight, and window-transparency values.

Windows High Contrast overrides all palettes with system colors. Windows 11 uses documented DWM rounded corners,
border color, dark mode, and transient backdrop support when available; Windows 10 uses the same layout and typography
with a solid panel fallback.

Existing INI files without `[Appearance] Mode` automatically load as `Custom`, so upgrades keep their previous visual
choices. New files and Reset use:

```ini
[Appearance]
Mode=System
```

`ShowProcessName` replaces `ShowColProcessName`. The old key is still accepted when the new key is absent, but AltTab
saves only the new name.

## Switcher layout

Fresh settings and Reset use the horizontal `Dock` layout. It presents one icon per window in a centered rail with an
attached title/process caption, horizontal overflow without a visible scrollbar, and inline search text and result
count. The original `List` layout remains available; existing INIs without `SwitcherLayout` continue using it.

Dock mode supports `Default`, `Small`, and `Extra small` size presets. The setting scales Dock icons, tiles, caption,
fonts, and overall geometry only; List mode is unchanged. `Dock position` selects lower-third or centered placement,
while `Window Width (%)` remains the maximum Dock width.

```ini
[General]
SwitcherLayout=Dock
DockPosition=LowerThird
DockScale=Default
```

## Monitor placement

The Settings dialog's `Switcher monitor` dropdown controls where the switcher opens. `Automatic (foreground
application)` is the default and follows the monitor containing the foreground application. Choosing a specific display
keeps the switcher centered on that physical monitor, using that monitor's work area and DPI.

Fixed displays are saved by their Windows device identity. If the selected display is disconnected, AltTab temporarily
uses the primary monitor and returns to the selected display when it reconnects. The equivalent default INI setting is:

```ini
[General]
SwitcherMonitor=Automatic
```

# Features
* Find the right window faster (filter windows using search string), uses fuzzy string matching algorithm (no need to type the exact search string).
* Switch between windows of the same application using Alt + \` (Grave Accent / Backtick, the key right above the Tab on a US English keyboard layout).
* Terminate a process or all processes either normally or forcefully using keyboard shortcuts.
* Hide or unhide windows.
* Native, DPI-aware light/dark switcher with rounded search and selection surfaces.
* Horizontal icon Dock and vertical List layouts, with three Dock-only size presets.
* Themed native tray and window menus with keyboard navigation and submenu behavior intact.
* INI configuration for theme mode, Custom-mode fonts/colors/transparency, width, height, and behavior.

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## UI implementation

The switcher keeps native `EDIT` and `SysListView32` controls for IME, accessibility, scrolling, selection, and input.
The Dock hides the input caret and paints its query in the caption surface. A small GDI renderer owns its fonts and
brushes and paints the modern surfaces, rows/tiles, match emphasis, and vector close glyph.
Popup menus remain native `HMENU` trees and use a shared owner-draw session, preserving native focus, dismissal, keyboard
navigation, and command dispatch.

[gh-downloads]: https://img.shields.io/github/downloads/lokeshgovindu/AltTab/total?color=pink&label=GitHub%20Downloads
