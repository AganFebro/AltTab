#include "PreCompile.h"
#include "AltTabWindowRenderer.h"

#include "AltTabSettings.h"
#include "AltTabWindow.h"

#include <algorithm>
#include <utility>

namespace {

    HFONT CreateThemeFont(const std::wstring& face, int points, int weight, bool italic, UINT dpi) {
        return CreateFontW(
            -MulDiv(points, static_cast<int>(dpi), 72),
            0,
            0,
            0,
            weight,
            italic,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            face.c_str());
    }

    void DeleteFont(HFONT& font) {
        if (font)
            DeleteObject(std::exchange(font, nullptr));
    }

    void DeleteBrush(HBRUSH& brush) {
        if (brush)
            DeleteObject(std::exchange(brush, nullptr));
    }

    void FillRounded(HDC hdc, const RECT& rect, int radius, HBRUSH brush) {
        if (!brush || rect.right <= rect.left || rect.bottom <= rect.top)
            return;
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    std::wstring Ellipsize(HDC hdc, const std::wstring& text, int availableWidth) {
        if (text.empty() || availableWidth <= 0)
            return {};
        SIZE size{};
        GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
        if (size.cx <= availableWidth)
            return text;

        constexpr wchar_t ellipsis[] = L"\u2026";
        SIZE ellipsisSize{};
        GetTextExtentPoint32W(hdc, ellipsis, 1, &ellipsisSize);
        if (ellipsisSize.cx > availableWidth)
            return {};

        size_t low = 0;
        size_t high = text.size();
        while (low < high) {
            const size_t middle = (low + high + 1) / 2;
            GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(middle), &size);
            if (size.cx + ellipsisSize.cx <= availableWidth)
                low = middle;
            else
                high = middle - 1;
        }
        // Do not truncate between the UTF-16 halves of an emoji or another
        // supplementary-plane character.
        if (low > 0 && low < text.size() && IS_HIGH_SURROGATE(text[low - 1]) && IS_LOW_SURROGATE(text[low]))
            --low;
        return text.substr(0, low) + ellipsis;
    }

    bool IsHighlighted(size_t index, const std::set<std::pair<size_t, size_t>>& highlights) {
        for (const auto& [start, end] : highlights) {
            if (index >= start && index <= end)
                return true;
            if (start > index)
                break;
        }
        return false;
    }

} // namespace

SwitcherRenderer::~SwitcherRenderer() {
    Reset();
}

void SwitcherRenderer::Reset() {
    DeleteFont(searchFont_);
    DeleteFont(titleFont_);
    DeleteFont(titleMatchFont_);
    DeleteFont(subtitleFont_);
    DeleteFont(subtitleMatchFont_);
    DeleteFont(menuFont_);
    DeleteBrush(panelBrush_);
    DeleteBrush(searchBrush_);
    DeleteBrush(rowBrush_);
    DeleteBrush(hoverBrush_);
    DeleteBrush(selectedBrush_);
    DeleteBrush(dangerBrush_);
}

void SwitcherRenderer::Rebuild(HWND owner, const AltTabSettings& settings, UINT dpi) {
    Reset();
    theme_ = ResolveTheme(settings, dpi);
    theme_.backdropAvailable = ApplyThemeToWindow(owner, theme_);

    const bool custom = settings.Appearance == AppearanceMode::Custom;
    const bool searchItalic = custom && settings.SSFontStyle.find(L"italic") != std::wstring::npos;
    const bool titleItalic = custom && settings.LVFontStyle.find(L"italic") != std::wstring::npos;
    const int searchWeight = custom && settings.SSFontStyle.find(L"bold") != std::wstring::npos ? FW_BOLD : FW_NORMAL;
    const int titleWeight = custom && settings.LVFontStyle.find(L"bold") != std::wstring::npos ? FW_BOLD : FW_SEMIBOLD;
    const int dockFontScale = settings.Layout == SwitcherLayout::Dock ? DockScalePercent(settings.DockSize) : 100;
    const int searchPoints = (std::max)(7, MulDiv(custom ? settings.SSFontSize : 11, dockFontScale, 100));
    const int titlePoints = (std::max)(7, MulDiv(custom ? settings.LVFontSize : 11, dockFontScale, 100));
    const int subtitlePoints =
        (std::max)(7, MulDiv(custom ? (std::max)(8, settings.LVFontSize - 2) : 9, dockFontScale, 100));

    searchFont_ = CreateThemeFont(theme_.searchFontName, searchPoints, searchWeight, searchItalic, theme_.dpi);
    titleFont_ = CreateThemeFont(theme_.titleFontName, titlePoints, titleWeight, titleItalic, theme_.dpi);
    titleMatchFont_ = CreateThemeFont(theme_.titleFontName, titlePoints, FW_BOLD, titleItalic, theme_.dpi);
    subtitleFont_ = CreateThemeFont(theme_.subtitleFontName, subtitlePoints, FW_NORMAL, titleItalic, theme_.dpi);
    subtitleMatchFont_ = CreateThemeFont(theme_.subtitleFontName, subtitlePoints, FW_SEMIBOLD, titleItalic, theme_.dpi);
    menuFont_ = CreateThemeFont(theme_.menuFontName, 10, FW_NORMAL, false, theme_.dpi);

    panelBrush_ = CreateSolidBrush(theme_.palette.panel);
    searchBrush_ = CreateSolidBrush(theme_.palette.search);
    rowBrush_ = CreateSolidBrush(theme_.palette.row);
    hoverBrush_ = CreateSolidBrush(theme_.palette.hover);
    selectedBrush_ = CreateSolidBrush(theme_.palette.selected);
    dangerBrush_ = CreateSolidBrush(BlendThemeColors(theme_.palette.danger, theme_.palette.panel, 0.26));
}

void SwitcherRenderer::PaintPanel(HWND, HDC hdc, const RECT& client, bool showSearch, SwitcherLayout layout) const {
    FillRect(hdc, &client, panelBrush_);
    if (layout == SwitcherLayout::Dock) {
        RECT caption{
            0,
            (std::min)(static_cast<int>(client.bottom), theme_.metrics.dockRailHeight),
            client.right,
            client.bottom,
        };
        FillRect(hdc, &caption, searchBrush_);
        return;
    }
    if (!showSearch)
        return;

    RECT searchRect{
        theme_.metrics.panelPadding,
        theme_.metrics.panelPadding,
        client.right - theme_.metrics.panelPadding,
        theme_.metrics.panelPadding + theme_.metrics.searchHeight,
    };
    FillRounded(hdc, searchRect, theme_.metrics.surfaceCornerRadius, searchBrush_);

    const int glyphSize = MulDiv(16, static_cast<int>(theme_.dpi), 96);
    const int left = searchRect.left + MulDiv(14, static_cast<int>(theme_.dpi), 96);
    const int top = searchRect.top + (theme_.metrics.searchHeight - glyphSize) / 2;
    HPEN pen =
        CreatePen(PS_SOLID, (std::max)(1, MulDiv(2, static_cast<int>(theme_.dpi), 96)), theme_.palette.secondaryText);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, left, top, left + glyphSize - 4, top + glyphSize - 4);
    MoveToEx(hdc, left + glyphSize - 5, top + glyphSize - 5, nullptr);
    LineTo(hdc, left + glyphSize, top + glyphSize);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

RECT SwitcherRenderer::DockCloseButtonRect(HWND listView, int itemIndex) const {
    RECT icon{};
    if (!listView || itemIndex < 0 || !ListView_GetItemRect(listView, itemIndex, &icon, LVIR_ICON))
        return {};
    const int tileLeft = (icon.left + icon.right - theme_.metrics.dockTileSize) / 2;
    const int tileTop = (theme_.metrics.dockRailHeight - theme_.metrics.dockTileSize) / 2;
    return {
        tileLeft + theme_.metrics.dockTileSize - theme_.metrics.dockCloseButtonSize,
        tileTop,
        tileLeft + theme_.metrics.dockTileSize,
        tileTop + theme_.metrics.dockCloseButtonSize,
    };
}

bool SwitcherRenderer::DrawDockItem(
    HWND listView,
    HDC hdc,
    int itemIndex,
    HIMAGELIST icons,
    const AltTabSettings& settings,
    int hotItem,
    bool closeHovered,
    RECT& closeHitRect) const {
    LVITEMW item{};
    item.mask = LVIF_PARAM | LVIF_IMAGE;
    item.iItem = itemIndex;
    if (!ListView_GetItem(listView, &item))
        return false;
    const auto* window = reinterpret_cast<const AltTabWindowData*>(item.lParam);
    if (!window)
        return false;

    RECT icon{};
    if (!ListView_GetItemRect(listView, itemIndex, &icon, LVIR_ICON))
        return false;
    RECT tile{
        (icon.left + icon.right - theme_.metrics.dockTileSize) / 2,
        (theme_.metrics.dockRailHeight - theme_.metrics.dockTileSize) / 2,
        (icon.left + icon.right + theme_.metrics.dockTileSize) / 2,
        (theme_.metrics.dockRailHeight + theme_.metrics.dockTileSize) / 2,
    };
    const bool selected = (ListView_GetItemState(listView, itemIndex, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    const bool hot = itemIndex == hotItem;
    if (window->IsBeingClosed)
        FillRounded(hdc, tile, theme_.metrics.surfaceCornerRadius, dangerBrush_);
    else if (selected)
        FillRounded(hdc, tile, theme_.metrics.surfaceCornerRadius, selectedBrush_);
    else if (hot && settings.ShowHighlightRect)
        FillRounded(hdc, tile, theme_.metrics.surfaceCornerRadius, hoverBrush_);

    if (icons && item.iImage >= 0) {
        const int iconX = (tile.left + tile.right - theme_.metrics.dockIconSize) / 2;
        const int iconY = (tile.top + tile.bottom - theme_.metrics.dockIconSize) / 2;
        ImageList_DrawEx(
            icons,
            item.iImage,
            hdc,
            iconX,
            iconY,
            theme_.metrics.dockIconSize,
            theme_.metrics.dockIconSize,
            CLR_NONE,
            CLR_NONE,
            ILD_TRANSPARENT);
    }

    if (selected) {
        const int indicatorWidth = MulDiv(16, static_cast<int>(theme_.dpi), 96);
        const int indicatorHeight = (std::max)(2, MulDiv(3, static_cast<int>(theme_.dpi), 96));
        RECT indicator{
            (tile.left + tile.right - indicatorWidth) / 2,
            tile.bottom - indicatorHeight - MulDiv(2, static_cast<int>(theme_.dpi), 96),
            (tile.left + tile.right + indicatorWidth) / 2,
            tile.bottom - MulDiv(2, static_cast<int>(theme_.dpi), 96),
        };
        HBRUSH accent = CreateSolidBrush(theme_.palette.accent);
        FillRounded(hdc, indicator, indicatorHeight, accent);
        DeleteObject(accent);
    }

    if (settings.ShowDeleteButton && hot) {
        closeHitRect = DockCloseButtonRect(listView, itemIndex);
        RECT closeSurface = closeHitRect;
        InflateRect(
            &closeSurface, -MulDiv(2, static_cast<int>(theme_.dpi), 96), -MulDiv(2, static_cast<int>(theme_.dpi), 96));
        if (closeHovered)
            FillRounded(hdc, closeSurface, theme_.metrics.surfaceCornerRadius, hoverBrush_);
        const int inset = theme_.metrics.dockCloseButtonSize / 3;
        HPEN pen = CreatePen(
            PS_SOLID,
            (std::max)(1, MulDiv(2, static_cast<int>(theme_.dpi), 96)),
            window->IsBeingClosed ? theme_.palette.danger : theme_.palette.secondaryText);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, closeHitRect.left + inset, closeHitRect.top + inset, nullptr);
        LineTo(hdc, closeHitRect.right - inset, closeHitRect.bottom - inset);
        MoveToEx(hdc, closeHitRect.right - inset, closeHitRect.top + inset, nullptr);
        LineTo(hdc, closeHitRect.left + inset, closeHitRect.bottom - inset);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
    return true;
}

void SwitcherRenderer::DrawDockCaption(
    HDC hdc,
    const RECT& rect,
    const AltTabWindowData* window,
    const AltTabSettings& settings,
    const std::wstring& searchText,
    int matchCount) const {
    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return;
    SetBkMode(hdc, TRANSPARENT);
    const int padding = theme_.metrics.panelPadding;
    RECT content{ rect.left + padding, rect.top, rect.right - padding, rect.bottom };

    if (!searchText.empty() && settings.ShowSearchString) {
        const std::wstring count = std::to_wstring(matchCount) + (matchCount == 1 ? L" match" : L" matches");
        HGDIOBJ oldFont = SelectObject(hdc, subtitleFont_);
        SIZE countSize{};
        GetTextExtentPoint32W(hdc, count.c_str(), static_cast<int>(count.size()), &countSize);
        SetTextColor(hdc, theme_.palette.secondaryText);
        RECT countRect{ content.right - countSize.cx, content.top, content.right, content.bottom };
        DrawTextW(hdc, count.c_str(), -1, &countRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        SelectObject(hdc, searchFont_);
        const int gap = theme_.metrics.panelPadding;
        const int queryWidth = (std::max)(0, static_cast<int>(countRect.left - content.left) - gap);
        std::wstring query = Ellipsize(hdc, searchText, queryWidth);
        RECT queryRect{ content.left, content.top, countRect.left - gap, content.bottom };
        SetTextColor(hdc, theme_.palette.primaryText);
        DrawTextW(hdc, query.c_str(), -1, &queryRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        return;
    }

    if (!window)
        return;
    std::wstring title = window->Title;
    if (window->IsConflictProcess)
        title += L"  [v" + window->Version + L"]";
    std::wstring process = settings.ShowProcessName ? L" \u2014 " + window->ProcessName : L"";
    const int available = content.right - content.left;

    HGDIOBJ oldFont = SelectObject(hdc, subtitleFont_);
    process = Ellipsize(hdc, process, available / 3);
    SIZE processSize{};
    GetTextExtentPoint32W(hdc, process.c_str(), static_cast<int>(process.size()), &processSize);

    SelectObject(hdc, window->TitleHighlights.empty() ? titleFont_ : titleMatchFont_);
    title = Ellipsize(hdc, title, available - processSize.cx);
    SIZE titleSize{};
    GetTextExtentPoint32W(hdc, title.c_str(), static_cast<int>(title.size()), &titleSize);
    int x = content.left + (available - titleSize.cx - processSize.cx) / 2;
    RECT titleRect{ x, content.top, x + titleSize.cx, content.bottom };
    SetTextColor(hdc, window->TitleHighlights.empty() ? theme_.palette.primaryText : theme_.palette.match);
    DrawTextW(hdc, title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    if (!process.empty()) {
        SelectObject(hdc, subtitleFont_);
        RECT processRect{ titleRect.right, content.top, titleRect.right + processSize.cx, content.bottom };
        SetTextColor(hdc, theme_.palette.secondaryText);
        DrawTextW(hdc, process.c_str(), -1, &processRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    SelectObject(hdc, oldFont);
}

void SwitcherRenderer::DrawHighlightedText(
    HDC hdc,
    RECT rect,
    const std::wstring& text,
    const std::set<std::pair<size_t, size_t>>& highlights,
    COLORREF normalColor,
    bool customMode,
    HFONT normalFont) const {
    HGDIOBJ originalFont = SelectObject(hdc, normalFont);
    const std::wstring fitted = Ellipsize(hdc, text, rect.right - rect.left);
    SetBkMode(hdc, TRANSPARENT);
    int x = rect.left;

    for (size_t index = 0; index < fitted.size();) {
        const bool highlighted = index < text.size() && IsHighlighted(index, highlights);
        size_t end = index + 1;
        while (end < fitted.size() && (end < text.size() && IsHighlighted(end, highlights)) == highlighted)
            ++end;
        const std::wstring_view segment(fitted.data() + index, end - index);
        SIZE extent{};
        GetTextExtentPoint32W(hdc, segment.data(), static_cast<int>(segment.size()), &extent);
        RECT segmentRect{ x, rect.top, (std::min)(rect.right, x + extent.cx), rect.bottom };

        if (highlighted && customMode) {
            HBRUSH matchBrush = CreateSolidBrush(theme_.palette.matchBackground);
            FillRect(hdc, &segmentRect, matchBrush);
            DeleteObject(matchBrush);
        }
        HFONT matchFont = normalFont == subtitleFont_ ? subtitleMatchFont_ : titleMatchFont_;
        SelectObject(hdc, highlighted && !customMode ? matchFont : normalFont);
        GetTextExtentPoint32W(hdc, segment.data(), static_cast<int>(segment.size()), &extent);
        segmentRect.right = (std::min)(rect.right, x + extent.cx);
        SetTextColor(hdc, highlighted ? theme_.palette.match : normalColor);
        DrawTextW(
            hdc, segment.data(), static_cast<int>(segment.size()), &segmentRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        x += extent.cx;
        index = end;
        if (x >= rect.right)
            break;
    }
    SelectObject(hdc, originalFont);
}

bool SwitcherRenderer::DrawListViewRow(
    HWND listView,
    const DRAWITEMSTRUCT& drawItem,
    HIMAGELIST icons,
    const AltTabSettings& settings,
    int hotRow,
    bool closeHovered,
    RECT& closeHitRect) const {
    const int rowIndex = static_cast<int>(drawItem.itemID);
    if (rowIndex < 0)
        return false;

    LVITEMW item{};
    item.mask = LVIF_PARAM | LVIF_IMAGE;
    item.iItem = rowIndex;
    if (!ListView_GetItem(listView, &item))
        return false;
    const auto* window = reinterpret_cast<const AltTabWindowData*>(item.lParam);
    if (!window)
        return false;

    RECT row = drawItem.rcItem;
    FillRect(drawItem.hDC, &row, rowBrush_);
    InflateRect(&row, -MulDiv(4, static_cast<int>(theme_.dpi), 96), -MulDiv(2, static_cast<int>(theme_.dpi), 96));
    const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
    const bool hot = rowIndex == hotRow || (drawItem.itemState & ODS_HOTLIGHT) != 0;
    if (window->IsBeingClosed)
        FillRounded(drawItem.hDC, row, theme_.metrics.surfaceCornerRadius, dangerBrush_);
    else if (selected)
        FillRounded(drawItem.hDC, row, theme_.metrics.surfaceCornerRadius, selectedBrush_);
    else if (hot && settings.ShowHighlightRect)
        FillRounded(drawItem.hDC, row, theme_.metrics.surfaceCornerRadius, hoverBrush_);

    const int iconX = row.left + theme_.metrics.rowHorizontalPadding;
    const int iconY = row.top + (row.bottom - row.top - theme_.metrics.iconSize) / 2;
    if (icons && item.iImage >= 0) {
        ImageList_DrawEx(
            icons,
            item.iImage,
            drawItem.hDC,
            iconX,
            iconY,
            theme_.metrics.iconSize,
            theme_.metrics.iconSize,
            CLR_NONE,
            CLR_NONE,
            ILD_TRANSPARENT);
    }

    closeHitRect = {};
    int textRight = row.right - theme_.metrics.rowHorizontalPadding;
    if (settings.ShowDeleteButton && hot) {
        closeHitRect = {
            row.right - theme_.metrics.rowHorizontalPadding - theme_.metrics.closeButtonSize,
            row.top + (row.bottom - row.top - theme_.metrics.closeButtonSize) / 2,
            row.right - theme_.metrics.rowHorizontalPadding,
            row.top + (row.bottom - row.top + theme_.metrics.closeButtonSize) / 2,
        };
        textRight = closeHitRect.left - theme_.metrics.rowHorizontalPadding / 2;
        if (closeHovered)
            FillRounded(drawItem.hDC, closeHitRect, theme_.metrics.surfaceCornerRadius, hoverBrush_);

        const int inset = theme_.metrics.closeButtonSize / 3;
        HPEN closePen = CreatePen(
            PS_SOLID,
            (std::max)(1, MulDiv(2, static_cast<int>(theme_.dpi), 96)),
            window->IsBeingClosed ? theme_.palette.danger : theme_.palette.secondaryText);
        HGDIOBJ oldPen = SelectObject(drawItem.hDC, closePen);
        MoveToEx(drawItem.hDC, closeHitRect.left + inset, closeHitRect.top + inset, nullptr);
        LineTo(drawItem.hDC, closeHitRect.right - inset, closeHitRect.bottom - inset);
        MoveToEx(drawItem.hDC, closeHitRect.right - inset, closeHitRect.top + inset, nullptr);
        LineTo(drawItem.hDC, closeHitRect.left + inset, closeHitRect.bottom - inset);
        SelectObject(drawItem.hDC, oldPen);
        DeleteObject(closePen);
    }

    const int textLeft = iconX + theme_.metrics.iconSize + theme_.metrics.rowHorizontalPadding;
    std::wstring title = window->Title;
    if (window->IsConflictProcess)
        title += L"  [v" + window->Version + L"]";
    const bool custom = settings.Appearance == AppearanceMode::Custom;
    const COLORREF primary =
        selected && theme_.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : theme_.palette.primaryText;
    const COLORREF secondary =
        selected && theme_.highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : theme_.palette.secondaryText;

    if (settings.ShowProcessName) {
        const int mid = (row.top + row.bottom) / 2;
        RECT titleRect{ textLeft, row.top + MulDiv(4, static_cast<int>(theme_.dpi), 96), textRight, mid + 2 };
        RECT subtitleRect{ textLeft, mid - 1, textRight, row.bottom - MulDiv(3, static_cast<int>(theme_.dpi), 96) };
        DrawHighlightedText(drawItem.hDC, titleRect, title, window->TitleHighlights, primary, custom, titleFont_);
        DrawHighlightedText(
            drawItem.hDC,
            subtitleRect,
            window->ProcessName,
            window->ProcessNameHighlights,
            secondary,
            custom,
            subtitleFont_);
    } else {
        RECT titleRect{ textLeft, row.top, textRight, row.bottom };
        DrawHighlightedText(drawItem.hDC, titleRect, title, window->TitleHighlights, primary, custom, titleFont_);
    }
    return true;
}
