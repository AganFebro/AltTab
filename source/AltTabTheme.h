#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

struct AltTabSettings;

enum class AppearanceMode {
    System,
    Light,
    Dark,
    Custom,
};

struct ThemePalette {
    COLORREF panel{};
    COLORREF search{};
    COLORREF row{};
    COLORREF hover{};
    COLORREF selected{};
    COLORREF border{};
    COLORREF primaryText{};
    COLORREF secondaryText{};
    COLORREF accent{};
    COLORREF match{};
    COLORREF matchBackground{};
    COLORREF danger{};
};

struct ThemeMetrics {
    int panelPadding{};
    int panelCornerRadius{};
    int surfaceCornerRadius{};
    int searchHeight{};
    int searchListGap{};
    int rowHeight{};
    int iconSize{};
    int rowHorizontalPadding{};
    int closeButtonSize{};
    int dockRailHeight{};
    int dockIconSize{};
    int dockTileSize{};
    int dockTileGap{};
    int dockCaptionHeight{};
    int dockCloseButtonSize{};
    int dockMinimumWidth{};
    int menuItemHeight{};
    int menuSeparatorHeight{};
};

struct ThemeSnapshot {
    AppearanceMode configuredMode{ AppearanceMode::System };
    AppearanceMode resolvedMode{ AppearanceMode::Light };
    bool dark{};
    bool highContrast{};
    bool backdropAvailable{};
    UINT dpi{ 96 };
    ThemePalette palette{};
    ThemeMetrics metrics{};
    std::wstring searchFontName;
    std::wstring titleFontName;
    std::wstring subtitleFontName;
    std::wstring menuFontName;
};

struct CustomThemeValues {
    COLORREF searchText{};
    COLORREF searchBackground{};
    COLORREF listText{};
    COLORREF listBackground{};
    COLORREF matchText{};
    COLORREF matchBackground{};
};

std::optional<AppearanceMode> ParseAppearanceMode(std::wstring_view value);
AppearanceMode ResolveAppearanceMode(std::wstring_view value, bool keyPresent, bool* invalid = nullptr);
std::wstring_view AppearanceModeName(AppearanceMode mode);
bool ResolveShowProcessNameSetting(std::optional<bool> currentValue, std::optional<bool> legacyValue);

bool IsSystemDarkMode();
bool IsHighContrastMode();
COLORREF GetWindowsAccentColor();
COLORREF BlendThemeColors(COLORREF foreground, COLORREF background, double foregroundAmount);
double ThemeContrastRatio(COLORREF first, COLORREF second);
ThemeMetrics ResolveThemeMetrics(UINT dpi, int dockScalePercent = 100);
ThemePalette ResolveThemePalette(
    AppearanceMode mode,
    bool systemDark,
    bool highContrast,
    COLORREF accent,
    const CustomThemeValues& custom);
ThemeSnapshot ResolveTheme(const AltTabSettings& settings, UINT dpi);

// Returns true when the system backdrop request succeeded. Every attribute is
// best-effort so the same code remains safe on Windows 10.
bool ApplyThemeToWindow(HWND hWnd, const ThemeSnapshot& theme);
