// Tests for which Tab presses at-terminal takes from Windows while its window is
// in front: Alt+Tab and Alt+Shift+Tab, each down with its up, and nothing else.

#include "pch.h"

#include <AtAltTab.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class AtAltTabTests
    {
        TEST_CLASS(AtAltTabTests);

        TEST_METHOD(AltTabInFrontIsTakenDownAndUp);
        TEST_METHOD(AltTabElsewhereStaysWindows);
        TEST_METHOD(CtrlOrWinWithAltTabStaysWindows);
        TEST_METHOD(TabWithoutAltIsLeftAlone);
        TEST_METHOD(UpFollowsItsDown);
        TEST_METHOD(OtherKeysAreLeftAlone);
    };

    void AtAltTabTests::AltTabInFrontIsTakenDownAndUp()
    {
        at_alt_tab::Gate gate;
        VERIFY_IS_TRUE(gate.Take(VK_TAB, true, true, false, false, true));
        VERIFY_IS_TRUE(gate.Take(VK_TAB, false, true, false, false, true));
    }

    void AtAltTabTests::AltTabElsewhereStaysWindows()
    {
        at_alt_tab::Gate gate;
        VERIFY_IS_FALSE(gate.Take(VK_TAB, true, true, false, false, false));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, false, true, false, false, false));
    }

    void AtAltTabTests::CtrlOrWinWithAltTabStaysWindows()
    {
        at_alt_tab::Gate gate;
        VERIFY_IS_FALSE(gate.Take(VK_TAB, true, true, true, false, true));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, false, true, true, false, true));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, true, true, false, true, true));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, false, true, false, true, true));
    }

    void AtAltTabTests::TabWithoutAltIsLeftAlone()
    {
        at_alt_tab::Gate gate;
        VERIFY_IS_FALSE(gate.Take(VK_TAB, true, false, false, false, true));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, false, false, false, false, true));
    }

    void AtAltTabTests::UpFollowsItsDown()
    {
        at_alt_tab::Gate gate;
        // Taken going down: its up is taken after Alt is let go and the window has lost the front.
        VERIFY_IS_TRUE(gate.Take(VK_TAB, true, true, false, false, true));
        VERIFY_IS_TRUE(gate.Take(VK_TAB, false, false, false, false, false));
        // Windows' going down: its up is Windows' even with Alt held and the window in front.
        VERIFY_IS_FALSE(gate.Take(VK_TAB, true, true, false, false, false));
        VERIFY_IS_FALSE(gate.Take(VK_TAB, false, true, false, false, true));
    }

    void AtAltTabTests::OtherKeysAreLeftAlone()
    {
        at_alt_tab::Gate gate;
        VERIFY_IS_FALSE(gate.Take(VK_ESCAPE, true, true, false, false, true));
        VERIFY_IS_FALSE(gate.Take(VK_SPACE, true, true, false, false, true));
        // Another key between a taken Tab's down and up doesn't lose the up.
        VERIFY_IS_TRUE(gate.Take(VK_TAB, true, true, false, false, true));
        VERIFY_IS_FALSE(gate.Take(VK_SHIFT, true, true, false, false, true));
        VERIFY_IS_TRUE(gate.Take(VK_TAB, false, true, false, false, true));
    }
}
