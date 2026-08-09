#include "AltTabLayout.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

namespace {

    void AssertShortDockItems(const RECT& work, UINT dpi, DockScale scale, std::size_t itemCount) {
        const DockGeometry geometry = ResolveDockGeometry(work, dpi, 45, itemCount, scale, DockPosition::Center);
        const int scalePercent = DockScalePercent(scale);
        const int tileSize = MulDiv(MulDiv(68, static_cast<int>(dpi), 96), scalePercent, 100);
        const int railHeight = MulDiv(MulDiv(76, static_cast<int>(dpi), 96), scalePercent, 100);
        const int windowWidth = geometry.window.right - geometry.window.left;
        const int expectedItemsWidth = static_cast<int>(itemCount) * tileSize
                                       + (static_cast<int>(itemCount) - 1) * (geometry.itemPitch - tileSize);

        assert(geometry.itemsWidth == expectedItemsWidth);
        assert(
            std::abs(
                (geometry.window.left + geometry.window.right)
                - (static_cast<int>(work.left) + static_cast<int>(work.right)))
            <= 1);

        RECT previous{};
        for (std::size_t index = 0; index < itemCount; ++index) {
            const RECT tile = ResolveDockTileRect(geometry, tileSize, railHeight, index);
            assert(tile.top == (railHeight - tileSize) / 2);
            assert(tile.left >= 0);
            assert(tile.right <= windowWidth);
            if (index > 0) {
                assert(tile.left - previous.left == geometry.itemPitch);
                assert(tile.left - previous.right == geometry.itemPitch - tileSize);
            }
            previous = tile;
        }

        const RECT first = ResolveDockTileRect(geometry, tileSize, railHeight, 0);
        const RECT last = ResolveDockTileRect(geometry, tileSize, railHeight, itemCount - 1);
        assert(std::abs(first.left - (windowWidth - last.right)) <= 1);
    }

} // namespace

int main() {
    assert(ParseSwitcherLayout(L"LIST") == SwitcherLayout::List);
    assert(ParseSwitcherLayout(L"dock") == SwitcherLayout::Dock);

    bool invalid = false;
    assert(ResolveSwitcherLayout(L"", false, &invalid) == SwitcherLayout::List && !invalid);
    assert(ResolveSwitcherLayout(L"grid", true, &invalid) == SwitcherLayout::Dock && invalid);
    assert(ParseDockPosition(L"lowerthird") == DockPosition::LowerThird);
    assert(ParseDockPosition(L"CENTER") == DockPosition::Center);
    assert(ResolveDockPosition(L"", &invalid) == DockPosition::LowerThird && !invalid);
    assert(ParseDockScale(L"DEFAULT") == DockScale::Default);
    assert(ParseDockScale(L"small") == DockScale::Small);
    assert(ParseDockScale(L"xsmall") == DockScale::ExtraSmall);
    assert(ResolveDockScale(L"", &invalid) == DockScale::Default && !invalid);
    assert(ResolveDockScale(L"large", &invalid) == DockScale::Default && invalid);
    assert(DockScalePercent(DockScale::Default) == 100);
    assert(DockScalePercent(DockScale::Small) == 85);
    assert(DockScalePercent(DockScale::ExtraSmall) == 70);

    const RECT work{ 0, 0, 1920, 1080 };
    const DockGeometry compact = ResolveDockGeometry(work, 96, 45, 2, DockScale::Default, DockPosition::LowerThird);
    assert(compact.window.right - compact.window.left == 280);
    assert(compact.window.bottom - compact.window.top == 114);
    assert((compact.window.top + compact.window.bottom) / 2 == 720);
    assert(compact.itemsWidth == 140);
    assert(compact.itemsLeft == 70);
    assert(!compact.overflow);

    const DockGeometry nineItems = ResolveDockGeometry(work, 96, 45, 9, DockScale::Default, DockPosition::LowerThird);
    assert(nineItems.itemsWidth == 644);
    assert(nineItems.window.right - nineItems.window.left == 668);
    assert(nineItems.itemsLeft == 12);
    assert(!nineItems.overflow);

    const DockGeometry centered = ResolveDockGeometry(work, 96, 45, 20, DockScale::Default, DockPosition::Center);
    assert(centered.window.right - centered.window.left == 864);
    assert((centered.window.top + centered.window.bottom) / 2 == 540);
    assert(centered.itemsLeft == 12);
    assert(centered.overflow);

    const DockGeometry dpi192 = ResolveDockGeometry(work, 192, 90, 1, DockScale::Default, DockPosition::Center);
    assert(dpi192.itemPitch == 144);
    assert(dpi192.window.bottom - dpi192.window.top == 228);

    const DockGeometry smallGeometry = ResolveDockGeometry(work, 96, 45, 2, DockScale::Small, DockPosition::Center);
    assert(smallGeometry.window.right - smallGeometry.window.left == 238);
    assert(smallGeometry.window.bottom - smallGeometry.window.top == 97);
    assert(smallGeometry.itemPitch == 61);

    const DockGeometry extraSmall = ResolveDockGeometry(work, 96, 45, 2, DockScale::ExtraSmall, DockPosition::Center);
    assert(extraSmall.window.right - extraSmall.window.left == 196);
    assert(extraSmall.window.bottom - extraSmall.window.top == 80);
    assert(extraSmall.itemPitch == 51);

    for (const RECT screen : {
             RECT{ 0, 0, 1366, 768 },
             RECT{ 0, 0, 1920, 1080 },
             RECT{ -2560, 0, 0, 1440 },
             RECT{ 0, 0, 3840, 2160 },
         }) {
        for (const UINT dpi : { 96U, 120U, 144U, 192U }) {
            for (const DockScale scale : { DockScale::Default, DockScale::Small, DockScale::ExtraSmall }) {
                AssertShortDockItems(screen, dpi, scale, 1);
                AssertShortDockItems(screen, dpi, scale, 2);
                AssertShortDockItems(screen, dpi, scale, 3);
            }
        }
    }

    const RECT firstTile = ResolveDockTileRect(compact, 68, 76, 0);
    const RECT secondTile = ResolveDockTileRect(compact, 68, 76, 1);
    assert(HitTestDockTile(compact, 68, 76, 2, 0, { firstTile.left + 1, firstTile.top + 1 }) == 0);
    assert(HitTestDockTile(compact, 68, 76, 2, 0, { secondTile.left + 1, secondTile.top + 1 }) == 1);
    assert(HitTestDockTile(compact, 68, 76, 2, 0, { firstTile.right + 1, firstTile.top + 1 }) == -1);
    const RECT scrolledTile = ResolveDockTileRect(centered, 68, 76, 8, 240);
    assert(HitTestDockTile(centered, 68, 76, 20, 240, { scrolledTile.left + 1, scrolledTile.top + 1 }) == 8);
    assert(HitTestDockTile(centered, 68, 76, 20, 240, { scrolledTile.right + 1, scrolledTile.top + 1 }) == -1);
    assert(ClampDockScrollOffset(-20, 900, 400) == 0);
    assert(ClampDockScrollOffset(250, 900, 400) == 250);
    assert(ClampDockScrollOffset(800, 900, 400) == 500);

    const RECT viewport{ 0, 0, 400, 76 };
    assert(ResolveDockRevealDelta({ 20, 0, 88, 68 }, viewport) == 0);
    assert(ResolveDockRevealDelta({ 420, 0, 488, 68 }, viewport) == 254);

    std::cout << "AltTab layout tests passed\n";
    return 0;
}
