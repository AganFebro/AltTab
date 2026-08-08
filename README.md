![GitHub Downloads][gh-downloads]

# AltTab
AltTab is a small application created in C++, Win32, is an alternative for windows native task switcher (Alt+Tab / Alt+Shift+Tab). 

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

Build output is kept under `out/`. Do not use the removed `.sln`, `.vcxproj`, or MSBuild packaging scripts.

## Usage

Run `AltTab.exe` once. The application stays in the notification area and opens the switcher with the configured hotkeys:

* `Alt+Tab` / `Alt+Shift+Tab`: switch between windows.
* `Alt+Backtick` / `Alt+Shift+Backtick`: switch within a similar-process group.
* `Alt+Ctrl+Tab`: show the switcher without immediately changing windows.
* Type a title or process name to filter the list.
* `Delete` closes normally; `Shift+Delete` force-terminates the selected window/process.
* `Escape` cancels the switcher.

`AltTabSettings.ini` is stored beside the executable. It controls appearance, hotkeys, process groups, and process exclusions. Reload it from the notification-area menu after manual edits.

# Features
* Find the right window faster (filter windows using search string), uses fuzzy string matching algorithm (no need to type the exact search string).
* Switch between windows of the same application using Alt + \` (Grave Accent / Backtick, the key right above the Tab on a US English keyboard layout).
* Terminate a process or all processes either normally or forcefully using keyboard shortcuts.
Hide / Un-hide windows.
* Provided configuration/setting INI file & UI to change font style, background color, window transparency, width and height.

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.

## Screenshots
### AltTab main window
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/AltTab.gif)
### AltTab Settings
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/6.Settings.png)
### AltBacktick
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/7.AltBacktick.png)
### AltBacktick with SimilarProcessGroups
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/8.SimilarProcessGroups.png)
### ContextMenu
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/9.0.ContextMenu.png)
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/9.1.ContextMenu.png)
### TrayMenu
![](https://github.com/lokeshgovindu/AltTab/blob/master/Screenshots/10.TrayMenu.png)

[gh-downloads]: https://img.shields.io/github/downloads/lokeshgovindu/AltTab/total?color=pink&label=GitHub%20Downloads
