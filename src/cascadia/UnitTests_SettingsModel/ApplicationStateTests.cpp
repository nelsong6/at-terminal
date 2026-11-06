// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include <fstream>
#include <til/mutex.h>
#include <til/throttled_func.h>

#include "../TerminalSettingsModel/ApplicationState.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    // Every field, with a value of the wrong type. A new field has to be added
    // here: ApplicationStateTests::WrongTypeInAnyField checks the count.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 8> wrongTypes{ {
        { "settingsHash", "5" },
        { "generatedProfiles", "true" },
        { "persistedWindowLayouts", "5" },
        { "recentCommands", "5" },
        { "allowedCommandlines", "5" },
        { "dismissedBadges", "5" },
        { "sshFolderGenerated", R"("yes")" },
        { "fontSizeDelta", R"("-8")" },
    } };

#define COUNT_FIELD(...) +1
    static constexpr size_t fieldCount{ 0 MTSM_APPLICATION_STATE_FIELDS(COUNT_FIELD) };
#undef COUNT_FIELD

    class ApplicationStateTests
    {
        TEST_CLASS(ApplicationStateTests);

        TEST_METHOD_SETUP(CreateFolder)
        {
            _folder = std::filesystem::temp_directory_path() / fmt::format(FMT_COMPILE(L"at-terminal-state-test-{}"), GetCurrentProcessId());
            std::filesystem::remove_all(_folder);
            return std::filesystem::create_directories(_folder);
        }

        TEST_METHOD_CLEANUP(RemoveFolder)
        {
            std::error_code ec;
            std::filesystem::remove_all(_folder, ec);
            return !ec;
        }

        TEST_METHOD(GoodFileLoadsWithoutProblems);
        TEST_METHOD(NullIsNoValue);
        TEST_METHOD(DismissedMessagesFromAnEarlierBuildIsIgnored);
        TEST_METHOD(TheIncidentFile);
        TEST_METHOD(WrongTypeInAnyField);
        TEST_METHOD(WrongTypeInEveryField);
        TEST_METHOD(BadEntriesInsideAValue);
        TEST_METHOD(FileThatIsNotAnObject);
        TEST_METHOD(FileThatIsNotJson);
        TEST_METHOD(FontSizeDeltaRange);
        TEST_METHOD(ProblemsAreTakenOnce);
        TEST_METHOD(RewriteDropsWhatWasIgnored);
        TEST_METHOD(ElevatedRewriteOfAFileThatIsNotAnObject);

    private:
        std::filesystem::path _folder;

        std::filesystem::path _statePath() const { return _folder / L"state.json"; }
        std::filesystem::path _badPath() const { return _folder / L"state.json.bad"; }

        void _write(const std::string_view content) const
        {
            std::ofstream file{ _statePath(), std::ios::binary };
            file << content;
        }

        static std::string _read(const std::filesystem::path& path)
        {
            std::ifstream file{ path, std::ios::binary };
            return { std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
        }

        // Not elevated unless asked: an elevated run skips the Local fields in
        // state.json, and whether this test runs elevated is up to the runner.
        winrt::com_ptr<implementation::ApplicationState> _load(const bool elevated = false) const
        {
            return winrt::make_self<implementation::ApplicationState>(_folder, elevated);
        }

        // A file with problems reports them, then says where its copy went,
        // and that copy is the file as it was.
        void _verifyProblems(const winrt::com_ptr<implementation::ApplicationState>& state, const uint32_t expected, const std::string_view original) const
        {
            const auto problems = state->TakeLoadProblems();
            for (const auto& problem : problems)
            {
                Log::Comment(problem.c_str());
            }
            VERIFY_ARE_EQUAL(expected + 1, problems.Size());

            const std::wstring kept{ problems.GetAt(expected) };
            VERIFY_IS_TRUE(kept.ends_with(_badPath().native()));
            VERIFY_IS_TRUE(_read(_badPath()) == original);
        }
    };

    void ApplicationStateTests::GoodFileLoadsWithoutProblems()
    {
        static constexpr std::string_view content{ R"({"settingsHash":"abc","sshFolderGenerated":true,"fontSizeDelta":-8})" };
        _write(content);

        {
            const auto state = _load();
            VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
            VERIFY_IS_TRUE(state->SettingsHash() == L"abc");
            VERIFY_IS_TRUE(state->SSHFolderGenerated());
            VERIFY_ARE_EQUAL(-8.f, state->FontSizeDelta());
        }

        VERIFY_IS_FALSE(std::filesystem::exists(_badPath()));
        VERIFY_IS_TRUE(_read(_statePath()) == content);
    }

    void ApplicationStateTests::NullIsNoValue()
    {
        _write(R"({"fontSizeDelta":null})");

        const auto state = _load();
        VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
        VERIFY_ARE_EQUAL(0.f, state->FontSizeDelta());
    }

    // The file that stopped at-terminal 1.25.10 opening on 2026-11-04.
    void ApplicationStateTests::TheIncidentFile()
    {
        static constexpr std::string_view content{ R"({
  "generatedProfiles": [
    "{ddfd2413-f7e5-595d-b707-028f3c8013f6}",
    "{2b9bc662-8476-535c-ac96-310d34724aca}",
    "{d4ff179a-86c6-55e8-affd-8834bd611c22}",
    "{7cefaa56-38f3-5cea-b010-b4ed69bff31f}"
  ],
  "settingsHash": "8d75290cb7e6e6a6-01dd6cab4c66da93",
  "sshFolderGenerated": true,
  "fontSizeDelta": "-8"
})" };
        _write(content);

        const auto state = _load();
        VERIFY_ARE_EQUAL(0.f, state->FontSizeDelta());
        VERIFY_ARE_EQUAL(size_t{ 4 }, state->GeneratedProfiles().size());
        VERIFY_IS_TRUE(state->SettingsHash() == L"8d75290cb7e6e6a6-01dd6cab4c66da93");
        VERIFY_IS_TRUE(state->SSHFolderGenerated());

        const auto problems = state->TakeLoadProblems();
        VERIFY_ARE_EQUAL(2u, problems.Size());
        VERIFY_IS_TRUE(problems.GetAt(0) == L"fontSizeDelta: expected number, found \"-8\"");
    }

    void ApplicationStateTests::WrongTypeInAnyField()
    {
        VERIFY_ARE_EQUAL(fieldCount, wrongTypes.size());

        for (const auto& [key, value] : wrongTypes)
        {
            Log::Comment(til::u8u16(key).c_str());
            const auto content = fmt::format(FMT_COMPILE("{{\"{}\":{}}}"), key, value);
            _write(content);
            _verifyProblems(_load(), 1, content);
        }
    }

    // Earlier builds kept this field, so a file holding it is not a bad file.
    void ApplicationStateTests::DismissedMessagesFromAnEarlierBuildIsIgnored()
    {
        _write(R"({"dismissedMessages":["closeOnExitInfo"],"fontSizeDelta":-2})");

        const auto state = _load();
        VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
        VERIFY_ARE_EQUAL(-2.f, state->FontSizeDelta());
    }

    void ApplicationStateTests::WrongTypeInEveryField()
    {
        std::string content{ "{" };
        for (const auto& [key, value] : wrongTypes)
        {
            fmt::format_to(std::back_inserter(content), FMT_COMPILE("\"{}\":{},"), key, value);
        }
        content.back() = '}';
        _write(content);

        _verifyProblems(_load(), gsl::narrow_cast<uint32_t>(wrongTypes.size()), content);
    }

    // These pass a type check, or sit deeper than one, and fail further in.
    void ApplicationStateTests::BadEntriesInsideAValue()
    {
        static constexpr std::array<std::string_view, 2> contents{
            R"({"generatedProfiles":["{zzzzzzzz-c2c6-5271-96e7-009a87ff44bf}"]})",
            R"({"persistedWindowLayouts":[{"launchMode":"nope"}]})",
        };

        for (const auto content : contents)
        {
            Log::Comment(til::u8u16(content).c_str());
            _write(content);
            _verifyProblems(_load(), 1, content);
        }
    }

    void ApplicationStateTests::FileThatIsNotAnObject()
    {
        static constexpr std::array<std::string_view, 3> contents{ "[1,2]", R"("text")", "5" };

        for (const auto content : contents)
        {
            Log::Comment(til::u8u16(content).c_str());
            _write(content);
            _verifyProblems(_load(), 1, content);
        }
    }

    void ApplicationStateTests::FileThatIsNotJson()
    {
        static constexpr std::string_view content{ R"({"fontSizeDelta": -8)" };
        _write(content);

        const auto state = _load();
        VERIFY_ARE_EQUAL(0.f, state->FontSizeDelta());
        _verifyProblems(state, 1, content);
    }

    void ApplicationStateTests::FontSizeDeltaRange()
    {
        static constexpr std::array<std::pair<std::string_view, float>, 3> good{ {
            { R"({"fontSizeDelta":0})", 0.f },
            { R"({"fontSizeDelta":-1024})", -1024.f },
            { R"({"fontSizeDelta":1024})", 1024.f },
        } };
        for (const auto& [content, expected] : good)
        {
            Log::Comment(til::u8u16(content).c_str());
            _write(content);
            const auto state = _load();
            VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
            VERIFY_ARE_EQUAL(expected, state->FontSizeDelta());
        }

        // The last is past what a float holds, so it arrives as infinity.
        static constexpr std::array<std::string_view, 3> bad{
            R"({"fontSizeDelta":1025})",
            R"({"fontSizeDelta":-100000})",
            R"({"fontSizeDelta":1e39})",
        };
        for (const auto content : bad)
        {
            Log::Comment(til::u8u16(content).c_str());
            _write(content);
            const auto state = _load();
            VERIFY_ARE_EQUAL(0.f, state->FontSizeDelta());
            _verifyProblems(state, 1, content);
        }
    }

    void ApplicationStateTests::ProblemsAreTakenOnce()
    {
        _write(R"({"fontSizeDelta":"-8"})");

        const auto state = _load();
        VERIFY_ARE_EQUAL(2u, state->TakeLoadProblems().Size());
        VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
    }

    // The next launch finds a clean file, and only the copy has the bad value.
    void ApplicationStateTests::RewriteDropsWhatWasIgnored()
    {
        static constexpr std::string_view content{ R"({"settingsHash":"abc","fontSizeDelta":"-8"})" };
        _write(content);

        {
            const auto state = _load();
            state->Flush();
        }

        VERIFY_IS_TRUE(_read(_badPath()) == content);
        const auto rewritten = _read(_statePath());
        VERIFY_IS_TRUE(rewritten.find("fontSizeDelta") == std::string::npos);

        const auto state = _load();
        VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
        VERIFY_IS_TRUE(state->SettingsHash() == L"abc");
    }

    // An elevated write layers its fields onto whatever state.json holds.
    void ApplicationStateTests::ElevatedRewriteOfAFileThatIsNotAnObject()
    {
        static constexpr std::string_view content{ "[1,2]" };
        _write(content);

        {
            const auto state = _load(true);
            _verifyProblems(state, 1, content);
            state->Flush();
        }

        const auto state = _load();
        VERIFY_ARE_EQUAL(0u, state->TakeLoadProblems().Size());
    }
}
