// Tests for reading at-terminal --offscreen's command line (AtOffscreen.h): the
// window's rectangle and the commandline it runs, and what is refused.

#include "pch.h"

#include <AtOffscreen.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class AtOffscreenTests
    {
        TEST_CLASS(AtOffscreenTests);

        TEST_METHOD(RectangleIsPositionAndSize);
        TEST_METHOD(RectangleMayBeLeftOfOrAboveTheScreens);
        TEST_METHOD(MalformedRectanglesAreRefused);
        TEST_METHOD(OrdinaryLaunchesAreNotOffscreen);
        TEST_METHOD(CommandlineFollowsTheRectangle);
        TEST_METHOD(NoCommandlineIsRefused);
    };

    void AtOffscreenTests::RectangleIsPositionAndSize()
    {
        const auto rect = at_offscreen::ParseRect(L"100,200,1920,1080");
        VERIFY_IS_TRUE(rect.has_value());
        VERIFY_ARE_EQUAL(100, rect->left);
        VERIFY_ARE_EQUAL(200, rect->top);
        VERIFY_ARE_EQUAL(2020, rect->right);
        VERIFY_ARE_EQUAL(1280, rect->bottom);
    }

    void AtOffscreenTests::RectangleMayBeLeftOfOrAboveTheScreens()
    {
        const auto rect = at_offscreen::ParseRect(L"-3840,-2200,3840,2160");
        VERIFY_IS_TRUE(rect.has_value());
        VERIFY_ARE_EQUAL(-3840, rect->left);
        VERIFY_ARE_EQUAL(-2200, rect->top);
        VERIFY_ARE_EQUAL(0, rect->right);
        VERIFY_ARE_EQUAL(-40, rect->bottom);
    }

    void AtOffscreenTests::MalformedRectanglesAreRefused()
    {
        for (const auto text : { L"", L"1,2,3", L"1,2,3,4,5", L"1,2,0,4", L"1,2,3,-4", L"a,2,3,4", L"1,,3,4", L"1,2,3,4 ", L"-,2,3,4", L"1,2,3,9999999" })
        {
            Log::Comment(text);
            VERIFY_IS_FALSE(at_offscreen::ParseRect(text).has_value());
        }
    }

    void AtOffscreenTests::OrdinaryLaunchesAreNotOffscreen()
    {
        RECT rect{};
        std::vector<std::wstring> commandline;
        VERIFY_IS_TRUE(at_offscreen::Parsed::NotOffscreen == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe" }, rect, commandline));
        VERIFY_IS_TRUE(at_offscreen::Parsed::NotOffscreen == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe", L"tmux", L"--offscreen" }, rect, commandline));
        VERIFY_IS_TRUE(at_offscreen::Parsed::NotOffscreen == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe", L"-Embedding" }, rect, commandline));
    }

    void AtOffscreenTests::CommandlineFollowsTheRectangle()
    {
        RECT rect{};
        std::vector<std::wstring> commandline;
        const std::vector<std::wstring> args{ L"at-terminal.exe", L"--offscreen", L"0,-2000,1920,1080", L"tmux.exe", L"-S", L"C:\\sock", L"attach" };
        VERIFY_IS_TRUE(at_offscreen::Parsed::Offscreen == at_offscreen::Parse(args, rect, commandline));
        VERIFY_ARE_EQUAL(-2000, rect.top);
        VERIFY_ARE_EQUAL(1920, rect.right);
        const std::vector<std::wstring> expected{ L"at-terminal.exe", L"new-tab", L"--", L"tmux.exe", L"-S", L"C:\\sock", L"attach" };
        VERIFY_IS_TRUE(commandline == expected);
    }

    void AtOffscreenTests::NoCommandlineIsRefused()
    {
        RECT rect{};
        std::vector<std::wstring> commandline;
        VERIFY_IS_TRUE(at_offscreen::Parsed::Refused == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe", L"--offscreen", L"0,0,10,10" }, rect, commandline));
        VERIFY_IS_TRUE(at_offscreen::Parsed::Refused == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe", L"--offscreen" }, rect, commandline));
        VERIFY_IS_TRUE(at_offscreen::Parsed::Refused == at_offscreen::Parse(std::vector<std::wstring>{ L"at-terminal.exe", L"--offscreen", L"bad", L"tmux.exe" }, rect, commandline));
    }
}
