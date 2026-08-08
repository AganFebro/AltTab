#pragma once

#include "AltTabTheme.h"

#include <memory>
#include <vector>

class ThemedMenuSession {
public:
    ThemedMenuSession(HMENU menu, const ThemeSnapshot& theme);
    ~ThemedMenuSession();

    ThemedMenuSession(const ThemedMenuSession&) = delete;
    ThemedMenuSession& operator=(const ThemedMenuSession&) = delete;

    HMENU Menu() const {
        return menu_;
    }
    HMENU Popup(UINT index = 0) const {
        return menu_ ? GetSubMenu(menu_, static_cast<int>(index)) : nullptr;
    }

    static bool HandleMeasureItem(MEASUREITEMSTRUCT* measureItem);
    static bool HandleDrawItem(const DRAWITEMSTRUCT* drawItem);

private:
    struct ItemData;

    void StyleMenu(HMENU menu);
    void DrawItem(const ItemData& item, const DRAWITEMSTRUCT& drawItem) const;

    HMENU menu_{};
    ThemeSnapshot theme_{};
    HFONT font_{};
    HBRUSH backgroundBrush_{};
    HBRUSH selectedBrush_{};
    std::vector<std::unique_ptr<ItemData>> items_;
};
