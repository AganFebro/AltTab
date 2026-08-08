#include "AltTabLayout.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

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
    assert(compact.itemsWidth == 144);
    assert(compact.itemsLeft == 68);
    assert(!compact.overflow);

    const DockGeometry nineItems = ResolveDockGeometry(work, 96, 45, 9, DockScale::Default, DockPosition::LowerThird);
    assert(nineItems.itemsWidth == 648);
    assert(nineItems.window.right - nineItems.window.left == 672);
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

    const RECT viewport{ 0, 0, 400, 76 };
    assert(ResolveDockRevealDelta({ 20, 0, 88, 68 }, viewport) == 0);
    assert(ResolveDockRevealDelta({ 420, 0, 488, 68 }, viewport) == 254);

    std::cout << "AltTab layout tests passed\n";
    return 0;
}
