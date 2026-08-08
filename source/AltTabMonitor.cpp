#include "AltTabMonitor.h"

#include <ShellScalingApi.h>

#include <algorithm>
#include <cwctype>
#include <format>
#include <unordered_map>

namespace {

    constexpr std::size_t NO_MONITOR = static_cast<std::size_t>(-1);

    struct DisplayIdentity {
        std::wstring persistentId;
        std::wstring friendlyName;
    };

    std::wstring ToLower(std::wstring_view value) {
        std::wstring result(value);
        std::transform(result.begin(), result.end(), result.begin(), towlower);
        return result;
    }

    bool EqualsIgnoreCase(std::wstring_view left, std::wstring_view right) {
        return ToLower(left) == ToLower(right);
    }

    std::unordered_map<std::wstring, DisplayIdentity> GetDisplayIdentities() {
        std::unordered_map<std::wstring, DisplayIdentity> identities;
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        LONG result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (result != ERROR_SUCCESS)
            return identities;

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        for (int attempt = 0; attempt < 3; ++attempt) {
            result =
                QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
            if (result != ERROR_INSUFFICIENT_BUFFER)
                break;
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
                return identities;
            paths.resize(pathCount);
            modes.resize(modeCount);
        }
        if (result != ERROR_SUCCESS)
            return identities;

        for (UINT32 index = 0; index < pathCount; ++index) {
            const DISPLAYCONFIG_PATH_INFO& path = paths[index];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = path.sourceInfo.adapterId;
            source.header.id = path.sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS || source.viewGdiDeviceName[0] == L'\0')
                continue;

            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target.header.size = sizeof(target);
            target.header.adapterId = path.targetInfo.adapterId;
            target.header.id = path.targetInfo.id;

            DisplayIdentity identity;
            if (DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS) {
                identity.persistentId = target.monitorDevicePath;
                identity.friendlyName = target.monitorFriendlyDeviceName;
            }
            identities.try_emplace(ToLower(source.viewGdiDeviceName), std::move(identity));
        }
        return identities;
    }

    BOOL CALLBACK CollectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM context) {
        auto& monitors = *reinterpret_cast<std::vector<MonitorDescriptor>*>(context);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(handle, &info))
            return TRUE;

        MonitorDescriptor monitor;
        monitor.handle = handle;
        monitor.gdiDeviceName = info.szDevice;
        monitor.persistentId = monitor.gdiDeviceName;
        monitor.monitorArea = info.rcMonitor;
        monitor.workArea = info.rcWork;
        monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        UINT dpiX = 96;
        UINT dpiY = 96;
        if (SUCCEEDED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
            monitor.dpi = dpiX;
        monitors.push_back(std::move(monitor));
        return TRUE;
    }

    MonitorDescriptor MakeSystemFallback() {
        MonitorDescriptor monitor;
        monitor.handle = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        monitor.persistentId = L"Primary";
        monitor.friendlyName = L"Primary monitor";
        monitor.monitorArea = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        monitor.workArea = monitor.monitorArea;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor.workArea, 0);
        monitor.primary = true;
        return monitor;
    }

} // namespace

bool IsAutomaticSwitcherMonitor(std::wstring_view setting) {
    return setting.empty() || EqualsIgnoreCase(setting, AUTOMATIC_SWITCHER_MONITOR);
}

bool MonitorMatchesSetting(const MonitorDescriptor& monitor, std::wstring_view setting) {
    return !setting.empty()
           && (EqualsIgnoreCase(monitor.persistentId, setting) || EqualsIgnoreCase(monitor.gdiDeviceName, setting));
}

std::size_t ResolveMonitorIndex(
    const std::vector<MonitorDescriptor>& monitors,
    std::wstring_view setting,
    HMONITOR automaticMonitor,
    bool* usedFallback) {
    if (usedFallback)
        *usedFallback = false;
    if (monitors.empty())
        return NO_MONITOR;

    if (IsAutomaticSwitcherMonitor(setting)) {
        const auto selected = std::find_if(monitors.begin(), monitors.end(), [automaticMonitor](const auto& monitor) {
            return monitor.handle == automaticMonitor;
        });
        if (selected != monitors.end())
            return static_cast<std::size_t>(selected - monitors.begin());
    } else {
        const auto selected = std::find_if(monitors.begin(), monitors.end(), [setting](const auto& monitor) {
            return MonitorMatchesSetting(monitor, setting);
        });
        if (selected != monitors.end())
            return static_cast<std::size_t>(selected - monitors.begin());
        if (usedFallback)
            *usedFallback = true;
    }

    const auto primary =
        std::find_if(monitors.begin(), monitors.end(), [](const auto& monitor) { return monitor.primary; });
    return primary == monitors.end() ? 0 : static_cast<std::size_t>(primary - monitors.begin());
}

std::vector<std::wstring> BuildMonitorLabels(const std::vector<MonitorDescriptor>& monitors) {
    std::vector<std::wstring> baseLabels;
    baseLabels.reserve(monitors.size());
    for (std::size_t index = 0; index < monitors.size(); ++index) {
        const MonitorDescriptor& monitor = monitors[index];
        const int width = monitor.monitorArea.right - monitor.monitorArea.left;
        const int height = monitor.monitorArea.bottom - monitor.monitorArea.top;
        const std::wstring name =
            monitor.friendlyName.empty() ? std::format(L"Monitor {}", index + 1) : monitor.friendlyName;
        baseLabels.push_back(
            std::format(L"{} - {} x {}{}", name, width, height, monitor.primary ? L" (Primary)" : L""));
    }

    std::unordered_map<std::wstring, std::size_t> totals;
    for (const auto& label : baseLabels)
        ++totals[label];

    std::unordered_map<std::wstring, std::size_t> occurrences;
    std::vector<std::wstring> labels;
    labels.reserve(baseLabels.size());
    for (auto& label : baseLabels) {
        const std::size_t occurrence = ++occurrences[label];
        labels.push_back(totals[label] > 1 ? std::format(L"{} ({})", label, occurrence) : std::move(label));
    }
    return labels;
}

std::vector<MonitorDescriptor> EnumerateMonitors() {
    std::vector<MonitorDescriptor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&monitors));

    const auto identities = GetDisplayIdentities();
    for (auto& monitor : monitors) {
        const auto identity = identities.find(ToLower(monitor.gdiDeviceName));
        if (identity == identities.end())
            continue;
        if (!identity->second.persistentId.empty())
            monitor.persistentId = identity->second.persistentId;
        monitor.friendlyName = identity->second.friendlyName;
    }

    std::sort(monitors.begin(), monitors.end(), [](const auto& left, const auto& right) {
        if (left.primary != right.primary)
            return left.primary;
        if (left.monitorArea.top != right.monitorArea.top)
            return left.monitorArea.top < right.monitorArea.top;
        if (left.monitorArea.left != right.monitorArea.left)
            return left.monitorArea.left < right.monitorArea.left;
        return left.gdiDeviceName < right.gdiDeviceName;
    });
    return monitors;
}

MonitorDescriptor ResolveSwitcherMonitor(std::wstring_view setting, HWND foregroundWindow, bool* usedFallback) {
    const std::vector<MonitorDescriptor> monitors = EnumerateMonitors();
    HWND anchor = IsWindow(foregroundWindow) ? foregroundWindow : GetForegroundWindow();
    const HMONITOR automaticMonitor = anchor ? MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST)
                                             : MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    const std::size_t selected = ResolveMonitorIndex(monitors, setting, automaticMonitor, usedFallback);
    return selected == NO_MONITOR ? MakeSystemFallback() : monitors[selected];
}
