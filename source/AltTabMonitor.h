#pragma once

#include <Windows.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::wstring_view AUTOMATIC_SWITCHER_MONITOR = L"Automatic";

struct MonitorDescriptor {
    HMONITOR handle{};
    std::wstring persistentId;
    std::wstring gdiDeviceName;
    std::wstring friendlyName;
    RECT monitorArea{};
    RECT workArea{};
    bool primary{};
    UINT dpi{ 96 };
};

bool IsAutomaticSwitcherMonitor(std::wstring_view setting);
bool MonitorMatchesSetting(const MonitorDescriptor& monitor, std::wstring_view setting);
std::size_t ResolveMonitorIndex(
    const std::vector<MonitorDescriptor>& monitors,
    std::wstring_view setting,
    HMONITOR automaticMonitor,
    bool* usedFallback = nullptr);
std::vector<std::wstring> BuildMonitorLabels(const std::vector<MonitorDescriptor>& monitors);
std::vector<MonitorDescriptor> EnumerateMonitors();
MonitorDescriptor
ResolveSwitcherMonitor(std::wstring_view setting, HWND foregroundWindow, bool* usedFallback = nullptr);
