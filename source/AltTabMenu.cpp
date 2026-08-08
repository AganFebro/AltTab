#include "PreCompile.h"
#include "AltTabMenu.h"

#include <algorithm>
#include <string>
#include <utility>

struct ThemedMenuSession::ItemData {
    ThemedMenuSession* owner{};
    std::wstring text;
    std::wstring accelerator;
    bool separator{};
    bool submenu{};
};

namespace {

    void FillRounded(HDC hdc, const RECT& rect, int radius, HBRUSH brush) {
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

} // namespace

ThemedMenuSession::ThemedMenuSession(HMENU menu, const ThemeSnapshot& theme)
    : menu_(menu)
    , theme_(theme) {
    font_ = CreateFontW(
        -MulDiv(10, static_cast<int>(theme_.dpi), 72),
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        theme_.menuFontName.c_str());
    backgroundBrush_ = CreateSolidBrush(theme_.palette.panel);
    selectedBrush_ = CreateSolidBrush(theme_.palette.hover);
    StyleMenu(menu_);
}

ThemedMenuSession::~ThemedMenuSession() {
    if (menu_)
        DestroyMenu(menu_);
    if (font_)
        DeleteObject(font_);
    if (backgroundBrush_)
        DeleteObject(backgroundBrush_);
    if (selectedBrush_)
        DeleteObject(selectedBrush_);
}

void ThemedMenuSession::StyleMenu(HMENU menu) {
    if (!menu)
        return;

    MENUINFO menuInfo{};
    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    menuInfo.hbrBack = backgroundBrush_;
    SetMenuInfo(menu, &menuInfo);

    const int count = GetMenuItemCount(menu);
    for (int position = 0; position < count; ++position) {
        MENUITEMINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING;
        GetMenuItemInfoW(menu, position, TRUE, &info);

        std::wstring text(info.cch + 1, L'\0');
        if (info.cch) {
            info.dwTypeData = text.data();
            info.cch = static_cast<UINT>(text.size());
            GetMenuItemInfoW(menu, position, TRUE, &info);
            text.resize(wcslen(text.c_str()));
        } else {
            text.clear();
        }

        auto item = std::make_unique<ItemData>();
        item->owner = this;
        item->separator = (info.fType & MFT_SEPARATOR) != 0;
        item->submenu = info.hSubMenu != nullptr;
        if (const size_t tab = text.find(L'\t'); tab != std::wstring::npos) {
            item->text = text.substr(0, tab);
            item->accelerator = text.substr(tab + 1);
        } else {
            item->text = std::move(text);
        }

        MENUITEMINFOW ownerDraw{};
        ownerDraw.cbSize = sizeof(ownerDraw);
        ownerDraw.fMask = MIIM_FTYPE | MIIM_DATA;
        ownerDraw.fType = info.fType | MFT_OWNERDRAW;
        ownerDraw.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
        SetMenuItemInfoW(menu, position, TRUE, &ownerDraw);

        if (info.hSubMenu)
            StyleMenu(info.hSubMenu);
        items_.push_back(std::move(item));
    }
}

bool ThemedMenuSession::HandleMeasureItem(MEASUREITEMSTRUCT* measureItem) {
    if (!measureItem || measureItem->CtlType != ODT_MENU || !measureItem->itemData)
        return false;
    const auto* item = reinterpret_cast<const ItemData*>(measureItem->itemData);
    const auto* owner = item->owner;
    if (!owner)
        return false;

    if (item->separator) {
        measureItem->itemHeight = owner->theme_.metrics.menuSeparatorHeight;
        measureItem->itemWidth = MulDiv(80, static_cast<int>(owner->theme_.dpi), 96);
        return true;
    }

    HDC hdc = GetDC(nullptr);
    HGDIOBJ oldFont = SelectObject(hdc, owner->font_);
    SIZE label{};
    SIZE accelerator{};
    GetTextExtentPoint32W(hdc, item->text.c_str(), static_cast<int>(item->text.size()), &label);
    GetTextExtentPoint32W(hdc, item->accelerator.c_str(), static_cast<int>(item->accelerator.size()), &accelerator);
    SelectObject(hdc, oldFont);
    ReleaseDC(nullptr, hdc);

    const int fixedSpace = MulDiv(68, static_cast<int>(owner->theme_.dpi), 96);
    const int acceleratorGap = item->accelerator.empty() ? 0 : MulDiv(28, static_cast<int>(owner->theme_.dpi), 96);
    measureItem->itemWidth = label.cx + accelerator.cx + fixedSpace + acceleratorGap;
    measureItem->itemHeight = owner->theme_.metrics.menuItemHeight;
    return true;
}

bool ThemedMenuSession::HandleDrawItem(const DRAWITEMSTRUCT* drawItem) {
    if (!drawItem || drawItem->CtlType != ODT_MENU || !drawItem->itemData)
        return false;
    const auto* item = reinterpret_cast<const ItemData*>(drawItem->itemData);
    if (!item->owner)
        return false;
    item->owner->DrawItem(*item, *drawItem);
    return true;
}

void ThemedMenuSession::DrawItem(const ItemData& item, const DRAWITEMSTRUCT& drawItem) const {
    HDC hdc = drawItem.hDC;
    RECT rect = drawItem.rcItem;
    FillRect(hdc, &rect, backgroundBrush_);

    if (item.separator) {
        const int inset = MulDiv(12, static_cast<int>(theme_.dpi), 96);
        const int y = (rect.top + rect.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, theme_.palette.border);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, rect.left + inset, y, nullptr);
        LineTo(hdc, rect.right - inset, y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        return;
    }

    const bool selected = (drawItem.itemState & ODS_SELECTED) != 0;
    const bool disabled = (drawItem.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    const bool checked = (drawItem.itemState & ODS_CHECKED) != 0;
    if (selected && !disabled) {
        RECT selection = rect;
        const int inset = MulDiv(3, static_cast<int>(theme_.dpi), 96);
        InflateRect(&selection, -inset, -inset);
        FillRounded(hdc, selection, theme_.metrics.surfaceCornerRadius, selectedBrush_);
    }

    const int gutter = MulDiv(34, static_cast<int>(theme_.dpi), 96);
    const int rightSpace = MulDiv(item.submenu ? 24 : 12, static_cast<int>(theme_.dpi), 96);
    RECT labelRect{ rect.left + gutter, rect.top, rect.right - rightSpace, rect.bottom };
    RECT accelRect = labelRect;
    const COLORREF textColor = disabled ? theme_.palette.secondaryText : theme_.palette.primaryText;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    HGDIOBJ oldFont = SelectObject(hdc, font_);
    const UINT cues = (drawItem.itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0;
    DrawTextW(hdc, item.text.c_str(), -1, &labelRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | cues);
    if (!item.accelerator.empty()) {
        DrawTextW(hdc, item.accelerator.c_str(), -1, &accelRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | cues);
    }
    SelectObject(hdc, oldFont);

    HPEN glyphPen = CreatePen(PS_SOLID, (std::max)(1, MulDiv(2, static_cast<int>(theme_.dpi), 96)), textColor);
    HGDIOBJ oldPen = SelectObject(hdc, glyphPen);
    if (checked) {
        const int x = rect.left + MulDiv(12, static_cast<int>(theme_.dpi), 96);
        const int y = (rect.top + rect.bottom) / 2;
        MoveToEx(hdc, x - MulDiv(4, static_cast<int>(theme_.dpi), 96), y, nullptr);
        LineTo(hdc, x - 1, y + MulDiv(4, static_cast<int>(theme_.dpi), 96));
        LineTo(hdc, x + MulDiv(6, static_cast<int>(theme_.dpi), 96), y - MulDiv(5, static_cast<int>(theme_.dpi), 96));
    }
    if (item.submenu) {
        const int x = rect.right - MulDiv(12, static_cast<int>(theme_.dpi), 96);
        const int y = (rect.top + rect.bottom) / 2;
        const int d = MulDiv(4, static_cast<int>(theme_.dpi), 96);
        MoveToEx(hdc, x - d, y - d, nullptr);
        LineTo(hdc, x, y);
        LineTo(hdc, x - d, y + d);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(glyphPen);
}
