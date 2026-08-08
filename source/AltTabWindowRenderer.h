#pragma once

#include "AltTabLayout.h"
#include "AltTabTheme.h"

#include <CommCtrl.h>
#include <set>
#include <string>

struct AltTabSettings;
struct AltTabWindowData;

class SwitcherRenderer {
public:
    SwitcherRenderer() = default;
    ~SwitcherRenderer();

    SwitcherRenderer(const SwitcherRenderer&) = delete;
    SwitcherRenderer& operator=(const SwitcherRenderer&) = delete;

    void Rebuild(HWND owner, const AltTabSettings& settings, UINT dpi);
    void Reset();

    void PaintPanel(HWND owner, HDC hdc, const RECT& client, bool showSearch, SwitcherLayout layout) const;
    bool DrawListViewRow(
        HWND listView,
        const DRAWITEMSTRUCT& drawItem,
        HIMAGELIST icons,
        const AltTabSettings& settings,
        int hotRow,
        bool closeHovered,
        RECT& closeHitRect) const;
    bool DrawDockItem(
        HWND listView,
        HDC hdc,
        int itemIndex,
        HIMAGELIST icons,
        const AltTabSettings& settings,
        int hotItem,
        bool closeHovered,
        RECT& closeHitRect) const;
    void DrawDockCaption(
        HDC hdc,
        const RECT& rect,
        const AltTabWindowData* window,
        const AltTabSettings& settings,
        const std::wstring& searchText,
        int matchCount) const;
    RECT DockCloseButtonRect(HWND listView, int itemIndex) const;

    const ThemeSnapshot& Theme() const {
        return theme_;
    }
    HFONT SearchFont() const {
        return searchFont_;
    }
    HFONT TitleFont() const {
        return titleFont_;
    }
    HFONT SubtitleFont() const {
        return subtitleFont_;
    }
    HFONT MenuFont() const {
        return menuFont_;
    }
    HBRUSH SearchBrush() const {
        return searchBrush_;
    }

private:
    void DrawHighlightedText(
        HDC hdc,
        RECT rect,
        const std::wstring& text,
        const std::set<std::pair<size_t, size_t>>& highlights,
        COLORREF normalColor,
        bool customMode,
        HFONT normalFont) const;

    ThemeSnapshot theme_{};
    HFONT searchFont_{};
    HFONT titleFont_{};
    HFONT titleMatchFont_{};
    HFONT subtitleFont_{};
    HFONT subtitleMatchFont_{};
    HFONT menuFont_{};
    HBRUSH panelBrush_{};
    HBRUSH searchBrush_{};
    HBRUSH rowBrush_{};
    HBRUSH hoverBrush_{};
    HBRUSH selectedBrush_{};
    HBRUSH dangerBrush_{};
};
