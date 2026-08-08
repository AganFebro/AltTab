#include "PreCompile.h"
#include "AltTabLayout.h"

#include <algorithm>
#include <cwctype>
#include <string>

namespace {

    std::wstring Lower(std::wstring_view value) {
        std::wstring result(value);
        std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) { return towlower(ch); });
        return result;
    }

} // namespace

std::optional<SwitcherLayout> ParseSwitcherLayout(std::wstring_view value) {
    const std::wstring normalized = Lower(value);
    if (normalized == L"list")
        return SwitcherLayout::List;
    if (normalized == L"dock")
        return SwitcherLayout::Dock;
    return std::nullopt;
}

SwitcherLayout ResolveSwitcherLayout(std::wstring_view value, bool keyPresent, bool* invalid) {
    if (invalid)
        *invalid = false;
    if (!keyPresent)
        return SwitcherLayout::List;
    if (const auto parsed = ParseSwitcherLayout(value))
        return *parsed;
    if (invalid)
        *invalid = true;
    return SwitcherLayout::Dock;
}

std::wstring_view SwitcherLayoutName(SwitcherLayout layout) {
    return layout == SwitcherLayout::List ? L"List" : L"Dock";
}

std::optional<DockPosition> ParseDockPosition(std::wstring_view value) {
    const std::wstring normalized = Lower(value);
    if (normalized == L"lowerthird")
        return DockPosition::LowerThird;
    if (normalized == L"center")
        return DockPosition::Center;
    return std::nullopt;
}

DockPosition ResolveDockPosition(std::wstring_view value, bool* invalid) {
    if (invalid)
        *invalid = false;
    if (const auto parsed = ParseDockPosition(value))
        return *parsed;
    if (invalid)
        *invalid = !value.empty();
    return DockPosition::LowerThird;
}

std::wstring_view DockPositionName(DockPosition position) {
    return position == DockPosition::Center ? L"Center" : L"LowerThird";
}

std::optional<DockScale> ParseDockScale(std::wstring_view value) {
    const std::wstring normalized = Lower(value);
    if (normalized == L"default")
        return DockScale::Default;
    if (normalized == L"small")
        return DockScale::Small;
    if (normalized == L"extrasmall" || normalized == L"xsmall")
        return DockScale::ExtraSmall;
    return std::nullopt;
}

DockScale ResolveDockScale(std::wstring_view value, bool* invalid) {
    if (invalid)
        *invalid = false;
    if (const auto parsed = ParseDockScale(value))
        return *parsed;
    if (invalid)
        *invalid = !value.empty();
    return DockScale::Default;
}

std::wstring_view DockScaleName(DockScale scale) {
    switch (scale) {
    case DockScale::Small:
        return L"Small";
    case DockScale::ExtraSmall:
        return L"ExtraSmall";
    default:
        return L"Default";
    }
}

int DockScalePercent(DockScale scale) {
    switch (scale) {
    case DockScale::Small:
        return 85;
    case DockScale::ExtraSmall:
        return 70;
    default:
        return 100;
    }
}

DockGeometry ResolveDockGeometry(
    const RECT& workArea,
    UINT dpi,
    int widthPercentage,
    std::size_t itemCount,
    DockScale dockScale,
    DockPosition position) {
    const auto scale = [dpi](int value) { return MulDiv(value, static_cast<int>(dpi ? dpi : 96), 96); };
    const auto scaleDock = [scale, dockScale](int value) {
        return MulDiv(scale(value), DockScalePercent(dockScale), 100);
    };
    const int workWidth = (std::max)(1L, workArea.right - workArea.left);
    const int workHeight = (std::max)(1L, workArea.bottom - workArea.top);
    const int railHeight = scaleDock(76);
    const int captionHeight = scaleDock(38);
    const int panelPadding = scaleDock(12);
    const int pitch = scaleDock(68) + scaleDock(4);
    const int minimumWidth = scaleDock(280);
    const int requestedMaximum = MulDiv(workWidth, std::clamp(widthPercentage, 10, 100), 100);
    const int maximumWidth = std::clamp(requestedMaximum, (std::min)(minimumWidth, workWidth), workWidth);
    // Native icon view arranges by full spacing cells; subtracting the trailing
    // visual gap makes the final item wrap into a clipped second row.
    const int itemsWidth = static_cast<int>(itemCount) * pitch;
    const int contentWidth = panelPadding * 2 + itemsWidth;
    const int windowWidth = (std::min)(maximumWidth, (std::max)(minimumWidth, contentWidth));
    const int windowHeight = (std::min)(workHeight, railHeight + captionHeight);
    const int centerY =
        position == DockPosition::Center ? workArea.top + workHeight / 2 : workArea.top + MulDiv(workHeight, 2, 3);
    const int x = workArea.left + (workWidth - windowWidth) / 2;
    const int y = std::clamp(
        centerY - windowHeight / 2, static_cast<int>(workArea.top), static_cast<int>(workArea.bottom) - windowHeight);

    DockGeometry geometry{};
    geometry.window = { x, y, x + windowWidth, y + windowHeight };
    geometry.iconViewport = { 0, 0, windowWidth, (std::min)(railHeight, windowHeight) };
    geometry.caption = { 0, geometry.iconViewport.bottom, windowWidth, windowHeight };
    geometry.itemPitch = pitch;
    geometry.itemsWidth = itemsWidth;
    geometry.itemsLeft = contentWidth > windowWidth ? panelPadding : (windowWidth - itemsWidth) / 2;
    geometry.contentWidth = contentWidth;
    geometry.overflow = contentWidth > windowWidth;
    return geometry;
}

int ResolveDockRevealDelta(const RECT& item, const RECT& viewport) {
    if (item.left >= viewport.left && item.right <= viewport.right)
        return 0;
    return (item.left + item.right) / 2 - (viewport.left + viewport.right) / 2;
}
