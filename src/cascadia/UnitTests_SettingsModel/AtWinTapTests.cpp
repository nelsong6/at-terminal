// Tests for the bare Win tap (ATT-16): which Win presses are masked from Start
// and which releases summon at-terminal.

#include "pch.h"

#include <AtWinTap.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using at_win_tap::Act;

namespace SettingsModelUnitTests
{
    class AtWinTapTests
    {
        TEST_CLASS(AtWinTapTests);

        TEST_METHOD(TapIsMaskedAndSummons);
        TEST_METHOD(RightWinTapsToo);
        TEST_METHOD(HeldWinRepeatsStillTap);
        TEST_METHOD(WinWithAnotherKeyStaysWindows);
        TEST_METHOD(WinWithModifierStaysWindows);
        TEST_METHOD(BothWinKeysStayWindows);
        TEST_METHOD(KeyReleasesDontSpoilTap);
        TEST_METHOD(MissedReleaseRecovers);
        TEST_METHOD(OnlyWinBacktickIsTheChord);
    };

    void AtWinTapTests::TapIsMaskedAndSummons()
    {
        at_win_tap::Gate gate;
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
        // And again: the gate starts over after each tap.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::RightWinTapsToo()
    {
        at_win_tap::Gate gate;
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_RWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_RWIN, false, true, false)));
    }

    void AtWinTapTests::HeldWinRepeatsStillTap()
    {
        at_win_tap::Gate gate;
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, true, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, true, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::WinWithAnotherKeyStaysWindows()
    {
        at_win_tap::Gate gate;
        // Win+L
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key('L', true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key('L', false, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
        // Win+Shift+S: Shift pressed after Win.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LSHIFT, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LSHIFT, false, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::WinWithModifierStaysWindows()
    {
        at_win_tap::Gate gate;
        // Ctrl, Alt or Shift already held when Win goes down: not masked, so Start behaves as Windows has it.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, true, false, true)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, false, true, true)));
        // Released before Win comes up: still not a tap.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, true, false, true)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::BothWinKeysStayWindows()
    {
        at_win_tap::Gate gate;
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_RWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_RWIN, true, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_RWIN, false, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
        // And the next tap works.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::KeyReleasesDontSpoilTap()
    {
        at_win_tap::Gate gate;
        // A key let go while Win is down (pressed before it) isn't a new key.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key('A', false, true, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
    }

    void AtWinTapTests::MissedReleaseRecovers()
    {
        at_win_tap::Gate gate;
        // Win+L: the lock screen keeps Win's release from the hook.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::None), static_cast<int>(gate.Key('L', true, false, false)));
        // Back at the desktop, Win isn't down, so this press is fresh.
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Mask), static_cast<int>(gate.Key(VK_LWIN, true, false, false)));
        VERIFY_ARE_EQUAL(static_cast<int>(Act::Summon), static_cast<int>(gate.Key(VK_LWIN, false, true, false)));
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
