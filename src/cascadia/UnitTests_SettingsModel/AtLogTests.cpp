// Tests for at-terminal's log file. The rule that matters most: nothing past a
// program's name reaches a line, unless it's the tmux server's name.

#include "pch.h"

#include <fstream>
#include <regex>
#include <AtLog.h>

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

namespace SettingsModelUnitTests
{
    class AtLogTests
    {
        TEST_CLASS(AtLogTests);

        TEST_METHOD_SETUP(CreateFolder)
        {
            _folder = std::filesystem::temp_directory_path() / fmt::format(FMT_COMPILE(L"at-terminal-log-test-{}"), GetCurrentProcessId());
            std::filesystem::remove_all(_folder);
            return std::filesystem::create_directories(_folder);
        }

        TEST_METHOD_CLEANUP(RemoveFolder)
        {
            std::error_code ec;
            std::filesystem::remove_all(_folder, ec);
            return true;
        }

        TEST_METHOD(ALineStartsWithItsTimeAndZone);
        TEST_METHOD(ALineEndsAtTheLimitWithANewline);
        TEST_METHOD(TmuxShowsOnlyItsServer);
        TEST_METHOD(AnyOtherProgramShowsOnlyItsName);
        TEST_METHOD(AValueCantAddALine);
        TEST_METHOD(ARunSaysHowItEnded);
        TEST_METHOD(TheLogRollsPastItsLimit);

    private:
        std::filesystem::path _folder;
    };

    void AtLogTests::ALineStartsWithItsTimeAndZone()
    {
        const auto path = _folder / L"a.log";
        at_log::AppendLine(path.c_str(), "hello");
        at_log::AppendLine(path.c_str(), "again");

        const auto log = til::io::read_file_as_utf8_string_if_exists(path);
        const std::regex shape{ R"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}[+-]\d\d:\d\d \[\d+\] hello\n\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}[+-]\d\d:\d\d \[\d+\] again\n)" };
        VERIFY_IS_TRUE(std::regex_match(log, shape));
    }

    void AtLogTests::ALineEndsAtTheLimitWithANewline()
    {
        const auto path = _folder / L"a.log";
        at_log::AppendLine(path.c_str(), std::string(4096, 'x'));

        const auto log = til::io::read_file_as_utf8_string_if_exists(path);
        VERIFY_ARE_EQUAL(512u, log.size());
        VERIFY_ARE_EQUAL('\n', log.back());
    }

    void AtLogTests::TmuxShowsOnlyItsServer()
    {
        const auto ours = at_log::DescribeProgram(LR"("C:\Program Files\at-home\tmux\current\tmux.exe" -L at new -A -s primary)");
        VERIFY_ARE_EQUAL(std::string{ "program=tmux.exe server=at" }, ours);

        VERIFY_ARE_EQUAL(std::string{ "program=tmux server=unknown" }, at_log::DescribeProgram(L"tmux new -s hunter2"));
    }

    void AtLogTests::AnyOtherProgramShowsOnlyItsName()
    {
        VERIFY_ARE_EQUAL(std::string{ "program=pwsh.exe" }, at_log::DescribeProgram(LR"(C:\pwsh\pwsh.exe -L hunter2 -c "echo hunter2")"));
    }

    void AtLogTests::AValueCantAddALine()
    {
        VERIFY_ARE_EQUAL(std::string{ "a?b?c" }, std::string{ at_log::Word(L"a\nb c").data() });
        VERIFY_ARE_EQUAL(64u, strlen(at_log::Word(std::wstring(500, L'x')).data()));
    }

    void AtLogTests::ARunSaysHowItEnded()
    {
        VERIFY_IS_TRUE(at_log::SaidHowItEnded(""));
        VERIFY_IS_TRUE(at_log::SaidHowItEnded("t [1] start package=none\nt [1] exit\n"));
        VERIFY_IS_TRUE(at_log::SaidHowItEnded("t [1] start package=none\nt [1] session ending\n"));
        VERIFY_IS_TRUE(at_log::SaidHowItEnded("t [1] start package=none\nt [1] crash abort thread=2\n"));

        VERIFY_IS_FALSE(at_log::SaidHowItEnded("t [1] start package=none\n"));
        VERIFY_IS_FALSE(at_log::SaidHowItEnded("t [1] start package=none\nt [1] connection ended exit=0x00000001 closed-by=program\n"));
        VERIFY_IS_FALSE(at_log::SaidHowItEnded("t [1] start package=none\nt [1] exit\nt [2] start package=none\n"));
    }

    void AtLogTests::TheLogRollsPastItsLimit()
    {
        const auto path = _folder / L"a.log";
        const auto old = _folder / L"a.log.old";

        std::ofstream{ path, std::ios::binary } << std::string(at_log::rollAtBytes, 'x');
        VERIFY_ARE_EQUAL(at_log::rollAtBytes, at_log::ReadAndRoll(path.c_str()).size());
        VERIFY_IS_TRUE(std::filesystem::exists(path));

        std::ofstream{ path, std::ios::binary | std::ios::app } << 'x';
        VERIFY_ARE_EQUAL(at_log::rollAtBytes + 1, at_log::ReadAndRoll(path.c_str()).size());
        VERIFY_IS_FALSE(std::filesystem::exists(path));
        VERIFY_IS_TRUE(std::filesystem::exists(old));
    }
}
