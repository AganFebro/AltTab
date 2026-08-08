#include "AltTabMonitor.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>

namespace {

    HMONITOR Handle(std::uintptr_t value) {
        return reinterpret_cast<HMONITOR>(value);
    }

    MonitorDescriptor Monitor(
        std::uintptr_t handle,
        std::wstring id,
        std::wstring gdiName,
        std::wstring friendlyName,
        bool primary,
        RECT area = { 0, 0, 1920, 1080 }) {
        MonitorDescriptor monitor;
        monitor.handle = Handle(handle);
        monitor.persistentId = std::move(id);
        monitor.gdiDeviceName = std::move(gdiName);
        monitor.friendlyName = std::move(friendlyName);
        monitor.monitorArea = area;
        monitor.workArea = area;
        monitor.primary = primary;
        return monitor;
    }

} // namespace

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

    assert(IsAutomaticSwitcherMonitor(L""));
    assert(IsAutomaticSwitcherMonitor(L"automatic"));
    assert(!IsAutomaticSwitcherMonitor(L"monitor-id"));

    const std::vector<MonitorDescriptor> monitors{
        Monitor(1, L"primary-id", L"\\\\.\\DISPLAY1", L"Acme Panel", true),
        Monitor(2, L"secondary-id", L"\\\\.\\DISPLAY2", L"Acme Panel", false),
    };

    bool fallback = false;
    assert(ResolveMonitorIndex(monitors, L"Automatic", Handle(2), &fallback) == 1 && !fallback);
    assert(ResolveMonitorIndex(monitors, L"SECONDARY-ID", Handle(1), &fallback) == 1 && !fallback);
    assert(ResolveMonitorIndex(monitors, L"\\\\.\\display2", Handle(1), &fallback) == 1 && !fallback);
    assert(ResolveMonitorIndex(monitors, L"disconnected-id", Handle(2), &fallback) == 0 && fallback);

    const std::vector<MonitorDescriptor> noPrimary{
        Monitor(3, L"first", L"first-gdi", L"First", false),
        Monitor(4, L"second", L"second-gdi", L"Second", false),
    };
    assert(ResolveMonitorIndex(noPrimary, L"missing", Handle(4), &fallback) == 0 && fallback);

    const std::vector<MonitorDescriptor> duplicates{
        Monitor(5, L"one", L"one-gdi", L"Same Model", false),
        Monitor(6, L"two", L"two-gdi", L"Same Model", false),
        Monitor(7, L"three", L"three-gdi", L"", true, { 0, 0, 1280, 720 }),
    };
    const auto labels = BuildMonitorLabels(duplicates);
    assert(labels[0] == L"Same Model - 1920 x 1080 (1)");
    assert(labels[1] == L"Same Model - 1920 x 1080 (2)");
    assert(labels[2] == L"Monitor 3 - 1280 x 720 (Primary)");

    const auto connectedMonitors = EnumerateMonitors();
    const auto connectedLabels = BuildMonitorLabels(connectedMonitors);
    assert(connectedLabels.size() == connectedMonitors.size());
    for (std::size_t index = 0; index < connectedMonitors.size(); ++index)
        std::wcout << connectedLabels[index] << L" | " << connectedMonitors[index].dpi << L" DPI | "
                   << connectedMonitors[index].persistentId << L'\n';

    std::cout << "AltTab monitor tests passed\n";
    return 0;
}
