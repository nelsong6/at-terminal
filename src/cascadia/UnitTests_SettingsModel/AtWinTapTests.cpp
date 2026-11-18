// Tests for the bare Win tap (ATT-16): which Win releases end a tap, summoning
// at-terminal with Start kept shut.

#include "pch.h"

#include <AtWinTap.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class AtWinTapTests
    {
        TEST_CLASS(AtWinTapTests);

        TEST_METHOD(TapSummons);
        TEST_METHOD(RightWinTapsToo);
        TEST_METHOD(HeldWinRepeatsStillTap);
        TEST_METHOD(WinWithAnotherKeyStaysWindows);
        TEST_METHOD(WinWithModifierStaysWindows);
        TEST_METHOD(BothWinKeysStayWindows);
        TEST_METHOD(KeyReleasesDontSpoilTap);
        TEST_METHOD(MissedReleaseRecovers);
        TEST_METHOD(OnlyWinBacktickIsTheChord);
    };

    void AtWinTapTests::TapSummons()
    {
        at_win_tap::Gate gate;
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
        // And again: the gate starts over after each tap.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::RightWinTapsToo()
    {
        at_win_tap::Gate gate;
        VERIFY_IS_FALSE(gate.Tap(VK_RWIN, true, false, false));
        VERIFY_IS_TRUE(gate.Tap(VK_RWIN, false, true, false));
    }

    void AtWinTapTests::HeldWinRepeatsStillTap()
    {
        at_win_tap::Gate gate;
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, true, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, true, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::WinWithAnotherKeyStaysWindows()
    {
        at_win_tap::Gate gate;
        // Win+L
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap('L', true, false, false));
        VERIFY_IS_FALSE(gate.Tap('L', false, true, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, false, true, false));
        // Win+Shift+S: Shift pressed after Win.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LSHIFT, true, false, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LSHIFT, false, true, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::WinWithModifierStaysWindows()
    {
        at_win_tap::Gate gate;
        // Ctrl, Alt or Shift already held when Win goes down: Start behaves as Windows has it.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, true));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, false, true, true));
        // Released before Win comes up: still not a tap.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, true));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::BothWinKeysStayWindows()
    {
        at_win_tap::Gate gate;
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap(VK_RWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap(VK_RWIN, true, true, false));
        VERIFY_IS_FALSE(gate.Tap(VK_RWIN, false, true, false));
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, false, true, false));
        // And the next tap works.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::KeyReleasesDontSpoilTap()
    {
        at_win_tap::Gate gate;
        // A key let go while Win is down (pressed before it) isn't a new key.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap('A', false, true, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::MissedReleaseRecovers()
    {
        at_win_tap::Gate gate;
        // Win+L: the lock screen keeps Win's release from the hook.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_FALSE(gate.Tap('L', true, false, false));
        // Back at the desktop, Win isn't down, so this press is fresh.
        VERIFY_IS_FALSE(gate.Tap(VK_LWIN, true, false, false));
        VERIFY_IS_TRUE(gate.Tap(VK_LWIN, false, true, false));
    }

    void AtWinTapTests::OnlyWinBacktickIsTheChord()
    {
        VERIFY_IS_TRUE(at_win_tap::IsWinBacktick(false, false, false, true, VK_OEM_3));
        VERIFY_IS_FALSE(at_win_tap::IsWinBacktick(false, false, false, false, VK_OEM_3));
        VERIFY_IS_FALSE(at_win_tap::IsWinBacktick(true, false, false, true, VK_OEM_3));
        VERIFY_IS_FALSE(at_win_tap::IsWinBacktick(false, true, false, true, VK_OEM_3));
        VERIFY_IS_FALSE(at_win_tap::IsWinBacktick(false, false, true, true, VK_OEM_3));
        VERIFY_IS_FALSE(at_win_tap::IsWinBacktick(false, false, false, true, 'A'));
    }
}
