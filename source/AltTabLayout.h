#pragma once

#include <windows.h>

#include <cstddef>
#include <optional>
#include <string_view>

enum class SwitcherLayout {
    List,
    Dock,
};

enum class DockPosition {
    LowerThird,
    Center,
};

enum class DockScale {
    Default,
    Small,
    ExtraSmall,
};

struct DockGeometry {
    RECT window{};
    RECT iconViewport{};
    RECT caption{};
    int itemPitch{};
    int itemsLeft{};
    int itemsWidth{};
    int contentWidth{};
    bool overflow{};
};

std::optional<SwitcherLayout> ParseSwitcherLayout(std::wstring_view value);
SwitcherLayout ResolveSwitcherLayout(std::wstring_view value, bool keyPresent, bool* invalid = nullptr);
std::wstring_view SwitcherLayoutName(SwitcherLayout layout);

std::optional<DockPosition> ParseDockPosition(std::wstring_view value);
DockPosition ResolveDockPosition(std::wstring_view value, bool* invalid = nullptr);
std::wstring_view DockPositionName(DockPosition position);

std::optional<DockScale> ParseDockScale(std::wstring_view value);
DockScale ResolveDockScale(std::wstring_view value, bool* invalid = nullptr);
std::wstring_view DockScaleName(DockScale scale);
int DockScalePercent(DockScale scale);

DockGeometry ResolveDockGeometry(
    const RECT& workArea,
    UINT dpi,
    int widthPercentage,
    std::size_t itemCount,
    DockScale scale,
    DockPosition position);

RECT ResolveDockTileRect(
    const DockGeometry& geometry,
    int tileSize,
    int railHeight,
    std::size_t itemIndex,
    int scrollOffset = 0);
int HitTestDockTile(
    const DockGeometry& geometry,
    int tileSize,
    int railHeight,
    std::size_t itemCount,
    int scrollOffset,
    POINT point);
int ClampDockScrollOffset(int requestedOffset, int contentWidth, int viewportWidth);

// Returns the horizontal ListView scroll delta needed to reveal an item. The
// item is centered only when it has crossed a viewport edge.
int ResolveDockRevealDelta(const RECT& item, const RECT& viewport);
