#include "AltTabTheme.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>

namespace {

    constexpr COLORREF Color(unsigned red, unsigned green, unsigned blue) {
        return static_cast<COLORREF>(red | (green << 8) | (blue << 16));
    }

} // namespace

int main() {
    assert(ParseAppearanceMode(L"System") == AppearanceMode::System);
    assert(ParseAppearanceMode(L"LIGHT") == AppearanceMode::Light);
    assert(ParseAppearanceMode(L"dark") == AppearanceMode::Dark);
    assert(ParseAppearanceMode(L"CuStOm") == AppearanceMode::Custom);

    bool invalid = false;
    assert(ResolveAppearanceMode(L"", false, &invalid) == AppearanceMode::Custom);
    assert(!invalid);
    assert(ResolveAppearanceMode(L"neon", true, &invalid) == AppearanceMode::System);
    assert(invalid);

    assert(ResolveShowProcessNameSetting(true, false));
    assert(!ResolveShowProcessNameSetting(false, true));
    assert(!ResolveShowProcessNameSetting(std::nullopt, false));
    assert(ResolveShowProcessNameSetting(std::nullopt, std::nullopt));

    const ThemeMetrics dpi96 = ResolveThemeMetrics(96);
    const ThemeMetrics dpi120 = ResolveThemeMetrics(120);
    const ThemeMetrics dpi144 = ResolveThemeMetrics(144);
    const ThemeMetrics dpi192 = ResolveThemeMetrics(192);
    const ThemeMetrics dockSmall = ResolveThemeMetrics(96, 85);
    const ThemeMetrics dockExtraSmall = ResolveThemeMetrics(96, 70);
    assert(dpi96.rowHeight == 52 && dpi96.iconSize == 32);
    assert(dpi96.dockRailHeight == 76 && dpi96.dockIconSize == 52 && dpi96.dockCaptionHeight == 38);
    assert(dpi120.rowHeight == 65 && dpi120.iconSize == 40);
    assert(dpi144.rowHeight == 78 && dpi144.iconSize == 48);
    assert(dpi192.rowHeight == 104 && dpi192.iconSize == 64);
    assert(dockSmall.rowHeight == 52 && dockSmall.dockRailHeight == 65 && dockSmall.dockIconSize == 44);
    assert(dockExtraSmall.rowHeight == 52 && dockExtraSmall.dockRailHeight == 53 && dockExtraSmall.dockIconSize == 36);

    const COLORREF accent = Color(0x00, 0x78, 0xd4);
    const CustomThemeValues custom{
        Color(1, 2, 3), Color(4, 5, 6), Color(7, 8, 9), Color(10, 11, 12), Color(13, 14, 15), Color(16, 17, 18),
    };
    const ThemePalette light = ResolveThemePalette(AppearanceMode::Light, false, false, accent, custom);
    const ThemePalette dark = ResolveThemePalette(AppearanceMode::Dark, false, false, accent, custom);
    const ThemePalette customPalette = ResolveThemePalette(AppearanceMode::Custom, false, false, accent, custom);
    const ThemePalette highContrast = ResolveThemePalette(AppearanceMode::Dark, true, true, accent, custom);
    assert(light.panel == Color(0xf6, 0xf7, 0xf9));
    assert(dark.panel == Color(0x17, 0x19, 0x1f));
    assert(customPalette.panel == custom.listBackground);
    assert(customPalette.search == custom.searchBackground);
    assert(highContrast.panel == GetSysColor(COLOR_WINDOW));
    assert(ThemeContrastRatio(light.selected, light.primaryText) >= 4.5);
    assert(ThemeContrastRatio(dark.selected, dark.primaryText) >= 4.5);

    const COLORREF blend = BlendThemeColors(Color(255, 255, 255), Color(0, 0, 0), 0.5);
    assert(std::abs(static_cast<int>(GetRValue(blend)) - 128) <= 1);
    assert(std::abs(static_cast<int>(GetGValue(blend)) - 128) <= 1);
    assert(std::abs(static_cast<int>(GetBValue(blend)) - 128) <= 1);

    std::cout << "AltTab theme tests passed\n";
    return 0;
}
