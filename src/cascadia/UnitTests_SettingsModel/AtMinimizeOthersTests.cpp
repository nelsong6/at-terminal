// Tests for which windows the summon key minimizes (ATT-13): another program's
// visible top-level window with a minimize button, and nothing else.

#include "pch.h"

#include <AtMinimizeOthers.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class AtMinimizeOthersTests
    {
        TEST_CLASS(AtMinimizeOthersTests);

        TEST_METHOD(OrdinaryWindowIsMinimized);
        TEST_METHOD(OwnWindowsStay);
        TEST_METHOD(HiddenOrAlreadyMinimizedAreLeftAlone);
        TEST_METHOD(WithoutMinimizeButtonStays);
        TEST_METHOD(ToolAndOwnedWindowsStay);
        TEST_METHOD(ShellStays);
        TEST_METHOD(CloakedByAppStays);
    };

    static at_minimize_others::Window Ordinary() noexcept
    {
        at_minimize_others::Window w;
        w.visible = true;
        w.style = WS_OVERLAPPEDWINDOW;
        return w;
    }

    void AtMinimizeOthersTests::OrdinaryWindowIsMinimized()
    {
        VERIFY_IS_TRUE(at_minimize_others::ShouldMinimize(Ordinary()));
    }

    void AtMinimizeOthersTests::OwnWindowsStay()
    {
        auto w = Ordinary();
        w.ours = true;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(w));
    }

    void AtMinimizeOthersTests::HiddenOrAlreadyMinimizedAreLeftAlone()
    {
        auto hidden = Ordinary();
        hidden.visible = false;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(hidden));

        auto minimized = Ordinary();
        minimized.minimized = true;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(minimized));
    }

    void AtMinimizeOthersTests::WithoutMinimizeButtonStays()
    {
        auto w = Ordinary();
        w.style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(w));
    }

    void AtMinimizeOthersTests::ToolAndOwnedWindowsStay()
    {
        auto tool = Ordinary();
        tool.exStyle = WS_EX_TOOLWINDOW;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(tool));

        auto owned = Ordinary();
        owned.owned = true;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(owned));
    }

    void AtMinimizeOthersTests::ShellStays()
    {
        auto w = Ordinary();
        w.shell = true;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(w));
    }

    void AtMinimizeOthersTests::CloakedByAppStays()
    {
        auto w = Ordinary();
        w.cloakedByApp = true;
        VERIFY_IS_FALSE(at_minimize_others::ShouldMinimize(w));
    }
}
