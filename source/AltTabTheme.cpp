#include "PreCompile.h"
#include "AltTabTheme.h"

#include "AltTabSettings.h"

#include <algorithm>
#include <cmath>
#include <dwmapi.h>

namespace {

    constexpr COLORREF Color(unsigned red, unsigned green, unsigned blue) {
        return static_cast<COLORREF>(red | (green << 8) | (blue << 16));
    }

    std::wstring Lower(std::wstring_view value) {
        std::wstring result(value);
        std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) { return towlower(ch); });
        return result;
    }

    double LinearChannel(BYTE value) {
        const double component = value / 255.0;
        return component <= 0.04045 ? component / 12.92 : std::pow((component + 0.055) / 1.055, 2.4);
    }

    double Luminance(COLORREF color) {
        return 0.2126 * LinearChannel(GetRValue(color)) + 0.7152 * LinearChannel(GetGValue(color))
               + 0.0722 * LinearChannel(GetBValue(color));
    }

    COLORREF EnsureSelectionContrast(COLORREF selection, COLORREF text, COLORREF panel, COLORREF accent) {
        if (ThemeContrastRatio(selection, text) >= 4.5) {
            return selection;
        }

        for (double amount = 0.25; amount >= 0.08; amount -= 0.03) {
            const COLORREF candidate = BlendThemeColors(accent, panel, amount);
            if (ThemeContrastRatio(candidate, text) >= 4.5) {
                return candidate;
            }
        }
        return panel;
    }

    int CALLBACK CountFont(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM found) {
        *reinterpret_cast<bool*>(found) = true;
        return 0;
    }

    bool IsFontAvailable(const wchar_t* face) {
        HDC hdc = GetDC(nullptr);
        if (!hdc)
            return false;
        LOGFONTW font{};
        font.lfCharSet = DEFAULT_CHARSET;
        wcsncpy_s(font.lfFaceName, face, _TRUNCATE);
        bool found = false;
        EnumFontFamiliesExW(hdc, &font, CountFont, reinterpret_cast<LPARAM>(&found), 0);
        ReleaseDC(nullptr, hdc);
        return found;
    }

} // namespace

std::optional<AppearanceMode> ParseAppearanceMode(std::wstring_view value) {
    const std::wstring normalized = Lower(value);
    if (normalized == L"system")
        return AppearanceMode::System;
    if (normalized == L"light")
        return AppearanceMode::Light;
    if (normalized == L"dark")
        return AppearanceMode::Dark;
    if (normalized == L"custom")
        return AppearanceMode::Custom;
    return std::nullopt;
}

AppearanceMode ResolveAppearanceMode(std::wstring_view value, bool keyPresent, bool* invalid) {
    if (invalid)
        *invalid = false;
    if (!keyPresent)
        return AppearanceMode::Custom;
    if (const auto parsed = ParseAppearanceMode(value))
        return *parsed;
    if (invalid)
        *invalid = true;
    return AppearanceMode::System;
}

std::wstring_view AppearanceModeName(AppearanceMode mode) {
    switch (mode) {
    case AppearanceMode::System:
        return L"System";
    case AppearanceMode::Light:
        return L"Light";
    case AppearanceMode::Dark:
        return L"Dark";
    case AppearanceMode::Custom:
        return L"Custom";
    }
    return L"System";
}

bool ResolveShowProcessNameSetting(std::optional<bool> currentValue, std::optional<bool> legacyValue) {
    if (currentValue)
        return *currentValue;
    if (legacyValue)
        return *legacyValue;
    return true;
}

bool IsSystemDarkMode() {
    DWORD useLightTheme = 1;
    DWORD size = sizeof(useLightTheme);
    const LSTATUS result = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &useLightTheme,
        &size);
    return result == ERROR_SUCCESS && useLightTheme == 0;
}

bool IsHighContrastMode() {
    HIGHCONTRASTW highContrast{};
    highContrast.cbSize = sizeof(highContrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0)
           && (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

COLORREF GetWindowsAccentColor() {
    DWORD colorization = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&colorization, &opaque))) {
        return Color((colorization >> 16) & 0xff, (colorization >> 8) & 0xff, colorization & 0xff);
    }
    return GetSysColor(COLOR_HIGHLIGHT);
}

COLORREF BlendThemeColors(COLORREF foreground, COLORREF background, double foregroundAmount) {
    const double amount = std::clamp(foregroundAmount, 0.0, 1.0);
    const auto blend = [amount](BYTE fg, BYTE bg) {
        return static_cast<unsigned>(std::lround(fg * amount + bg * (1.0 - amount)));
    };
    return Color(
        blend(GetRValue(foreground), GetRValue(background)),
        blend(GetGValue(foreground), GetGValue(background)),
        blend(GetBValue(foreground), GetBValue(background)));
}

double ThemeContrastRatio(COLORREF first, COLORREF second) {
    const double lighter = (std::max)(Luminance(first), Luminance(second));
    const double darker = (std::min)(Luminance(first), Luminance(second));
    return (lighter + 0.05) / (darker + 0.05);
}

ThemeMetrics ResolveThemeMetrics(UINT dpi) {
    const auto scale = [dpi](int value) { return MulDiv(value, static_cast<int>(dpi), 96); };
    return {
        scale(12), scale(12), scale(8),  scale(40), scale(8), scale(52),
        scale(32), scale(12), scale(28), scale(34), scale(9),
    };
}

ThemePalette ResolveThemePalette(
    AppearanceMode mode,
    bool systemDark,
    bool highContrast,
    COLORREF accent,
    const CustomThemeValues& custom) {
    if (highContrast) {
        return {
            GetSysColor(COLOR_WINDOW),        GetSysColor(COLOR_WINDOW),    GetSysColor(COLOR_WINDOW),
            GetSysColor(COLOR_HOTLIGHT),      GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_WINDOWFRAME),
            GetSysColor(COLOR_WINDOWTEXT),    GetSysColor(COLOR_GRAYTEXT),  GetSysColor(COLOR_HIGHLIGHT),
            GetSysColor(COLOR_HIGHLIGHTTEXT), GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT),
        };
    }

    if (mode == AppearanceMode::Custom) {
        return {
            custom.listBackground,
            custom.searchBackground,
            custom.listBackground,
            BlendThemeColors(custom.listText, custom.listBackground, 0.12),
            BlendThemeColors(accent, custom.listBackground, 0.38),
            BlendThemeColors(custom.listText, custom.listBackground, 0.28),
            custom.listText,
            BlendThemeColors(custom.listText, custom.listBackground, 0.68),
            accent,
            custom.matchText,
            custom.matchBackground,
            Color(196, 43, 28),
        };
    }

    const bool dark = mode == AppearanceMode::Dark || (mode == AppearanceMode::System && systemDark);
    ThemePalette palette{};
    if (dark) {
        palette = {
            Color(0x17, 0x19, 0x1f),
            Color(0x20, 0x23, 0x2b),
            Color(0x17, 0x19, 0x1f),
            Color(0x25, 0x29, 0x32),
            {},
            Color(0x45, 0x4a, 0x55),
            Color(0xf4, 0xf6, 0xfa),
            Color(0xa6, 0xac, 0xb7),
            accent,
            accent,
            Color(0x20, 0x23, 0x2b),
            Color(0xc9, 0x42, 0x42),
        };
        palette.selected = EnsureSelectionContrast(
            BlendThemeColors(accent, palette.panel, 0.34), palette.primaryText, palette.panel, accent);
    } else {
        palette = {
            Color(0xf6, 0xf7, 0xf9),
            Color(0xec, 0xef, 0xf3),
            Color(0xf6, 0xf7, 0xf9),
            Color(0xe8, 0xeb, 0xf0),
            {},
            Color(0xcd, 0xd1, 0xd8),
            Color(0x19, 0x1b, 0x1f),
            Color(0x5e, 0x63, 0x6c),
            accent,
            accent,
            Color(0xec, 0xef, 0xf3),
            Color(0xc4, 0x2b, 0x1c),
        };
        palette.selected = EnsureSelectionContrast(
            BlendThemeColors(accent, palette.panel, 0.22), palette.primaryText, palette.panel, accent);
    }
    return palette;
}

ThemeSnapshot ResolveTheme(const AltTabSettings& settings, UINT dpi) {
    ThemeSnapshot theme{};
    theme.configuredMode = settings.Appearance;
    theme.highContrast = IsHighContrastMode();
    const bool systemDark = IsSystemDarkMode();
    theme.resolvedMode = settings.Appearance == AppearanceMode::System
                             ? (systemDark ? AppearanceMode::Dark : AppearanceMode::Light)
                             : settings.Appearance;
    theme.dark = !theme.highContrast
                 && (theme.resolvedMode == AppearanceMode::Dark
                     || (theme.resolvedMode == AppearanceMode::Custom
                         && ThemeContrastRatio(settings.LVBackgroundColor, RGB(255, 255, 255))
                                > ThemeContrastRatio(settings.LVBackgroundColor, RGB(0, 0, 0))));
    theme.dpi = dpi == 0 ? 96 : dpi;
    theme.metrics = ResolveThemeMetrics(theme.dpi);
    const CustomThemeValues customValues{
        settings.SSFontColor,       settings.SSBackgroundColor,    settings.LVFontColor,
        settings.LVBackgroundColor, settings.LVHighlightTextColor, settings.LVHighlightBackgroundColor,
    };
    theme.palette =
        ResolveThemePalette(settings.Appearance, systemDark, theme.highContrast, GetWindowsAccentColor(), customValues);

    const bool custom = settings.Appearance == AppearanceMode::Custom;
    const wchar_t* modernFont = IsFontAvailable(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    theme.searchFontName = custom ? settings.SSFontName : modernFont;
    theme.titleFontName = custom ? settings.LVFontName : modernFont;
    theme.subtitleFontName = custom ? settings.LVFontName : modernFont;
    theme.menuFontName = modernFont;
    return theme;
}

bool ApplyThemeToWindow(HWND hWnd, const ThemeSnapshot& theme) {
    if (!hWnd)
        return false;

    const BOOL dark = theme.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    const DWM_WINDOW_CORNER_PREFERENCE corners = theme.highContrast ? DWMWCP_DEFAULT : DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

    const COLORREF border = theme.palette.border;
    DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &border, sizeof(border));

    const DWM_SYSTEMBACKDROP_TYPE backdrop =
        theme.highContrast || theme.configuredMode == AppearanceMode::Custom ? DWMSBT_NONE : DWMSBT_TRANSIENTWINDOW;
    return SUCCEEDED(DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop)));
}
