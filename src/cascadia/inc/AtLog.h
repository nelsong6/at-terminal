// at-terminal's log file, read through the home log pile (at-home-tools
// docs/app-logs.md). All of it lives here, so upstream code only gains one-line
// calls.
//
// A line holds fixed words, numbers and program names, never anything from the
// terminal: no output, input, title, folder, environment or full command line.
// This is the pane where passwords get typed, and every home PC's everyday NAS
// sign-in can read the pile.

#pragma once

#include <appmodel.h>
#include <csignal>
#include <shellapi.h>
#include <shlobj_core.h>
#include <til/io.h>

namespace at_log
{
    inline constexpr size_t rollAtBytes{ 1 << 20 };

    // Letters, digits and ._- only, so a value can't start a line or carry
    // anything but a name. Fixed size, because the crash paths must not allocate.
    inline std::array<char, 65> Word(const std::wstring_view text) noexcept
    {
        std::array<char, 65> word{};
        const auto length = std::min(text.size(), word.size() - 1);
        for (size_t i = 0; i < length; ++i)
        {
            const auto c = text[i];
            const auto safe = (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'_' || c == L'-';
            word[i] = safe ? static_cast<char>(c) : '?';
        }
        return word;
    }

    // One write, then close: Windows tells log-ship about a change only when the
    // writer flushes or closes. Failing to log must never fail the terminal.
    inline void AppendLine(const wchar_t* const path, const std::string_view text) noexcept
    {
        FILETIME utc{};
        FILETIME local{};
        SYSTEMTIME time{};
        GetSystemTimePreciseAsFileTime(&utc);
        FileTimeToLocalFileTime(&utc, &local);
        FileTimeToSystemTime(&local, &time);

        const auto ticks = [](const FILETIME& t) { return (static_cast<int64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
        const auto offsetMinutes = (ticks(local) - ticks(utc)) / 600000000;
        const auto offset = static_cast<int>(offsetMinutes < 0 ? -offsetMinutes : offsetMinutes);

        char line[512];
        const auto written = fmt::format_to_n(line,
                                              sizeof(line) - 1,
                                              FMT_COMPILE("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}{}{:02}:{:02} [{}] {}"),
                                              time.wYear,
                                              time.wMonth,
                                              time.wDay,
                                              time.wHour,
                                              time.wMinute,
                                              time.wSecond,
                                              time.wMilliseconds,
                                              offsetMinutes < 0 ? '-' : '+',
                                              offset / 60,
                                              offset % 60,
                                              GetCurrentProcessId(),
                                              text);
        auto size = std::min<size_t>(written.size, sizeof(line) - 1);
        line[size++] = '\n';

        const wil::unique_hfile file{ CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
        if (file)
        {
            DWORD ignored = 0;
            WriteFile(file.get(), line, gsl::narrow_cast<DWORD>(size), &ignored, nullptr);
        }
    }

    // The package's LocalState, beside settings.json. Each DLL works this out once
    // for itself, and a crash finds it already done.
    inline const wchar_t* Path() noexcept
    {
        static const auto path = []() noexcept {
            std::array<wchar_t, 1024> buffer{};
            wil::unique_cotaskmem_string folder;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_FORCE_APP_DATA_REDIRECTION, nullptr, &folder)))
            {
                swprintf_s(buffer.data(), buffer.size(), L"%s\\at-terminal.log", folder.get());
            }
            return buffer;
        }();
        return path.data();
    }

    inline void Write(const std::string_view text) noexcept
    {
        AppendLine(Path(), text);
    }

    // The tmux server's name, or just the program's for anything else. The rest of
    // the command line is left out: it can hold anything.
    inline std::string DescribeProgram(const wchar_t* const commandline)
    {
        int argc = 0;
        const wil::unique_hlocal_ptr<PWSTR[]> argv{ CommandLineToArgvW(commandline, &argc) };
        if (!argv || argc < 1)
        {
            return "program=unknown";
        }

        std::wstring_view program{ argv[0] };
        program = program.substr(program.find_last_of(L"\\/") + 1);
        auto description = fmt::format(FMT_COMPILE("program={}"), Word(program).data());
        if (til::equals_insensitive_ascii(program, L"tmux.exe") || til::equals_insensitive_ascii(program, L"tmux"))
        {
            const std::wstring_view server = argc > 2 && wcscmp(argv[1], L"-L") == 0 ? argv[2] : L"unknown";
            fmt::format_to(std::back_inserter(description), FMT_COMPILE(" server={}"), Word(server).data());
        }
        return description;
    }

    inline void Connected(const wchar_t* const commandline, const DWORD clientPid) noexcept
    try
    {
        Write(fmt::format(FMT_COMPILE("connected {} client-pid={}"), DescribeProgram(commandline), clientPid));
    }
    CATCH_LOG()

    inline void ConnectFailed(const wchar_t* const commandline, const HRESULT hr) noexcept
    try
    {
        Write(fmt::format(FMT_COMPILE("connect failed {} hr={:#010x}"), DescribeProgram(commandline), static_cast<uint32_t>(hr)));
    }
    CATCH_LOG()

    // STILL_ACTIVE (0x103) is the pseudoconsole going away under a program that is
    // still running.
    inline void ConnectionEnded(const DWORD exitCode, const bool closedByTerminal) noexcept
    try
    {
        Write(fmt::format(FMT_COMPILE("connection ended exit={:#010x} closed-by={}"), exitCode, closedByTerminal ? "terminal" : "program"));
    }
    CATCH_LOG()

    inline LPTOP_LEVEL_EXCEPTION_FILTER previousCrashFilter{};

    // A hardware fault or an uncaught SEH exception. A fail fast skips this, so all
    // that reports one is SaidHowItEnded at the next start.
    inline LONG WINAPI OnCrash(EXCEPTION_POINTERS* const info)
    {
        const auto& record = *info->ExceptionRecord;
        HMODULE module = nullptr;
        std::array<wchar_t, MAX_PATH> modulePath{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCWSTR>(record.ExceptionAddress), &module);
        const auto length = module ? GetModuleFileNameW(module, modulePath.data(), MAX_PATH) : 0;
        const std::wstring_view fullName{ modulePath.data(), length };
        const std::wstring_view name = length ? fullName.substr(fullName.find_last_of(L'\\') + 1) : L"unknown";
        const auto offset = static_cast<const char*>(record.ExceptionAddress) - reinterpret_cast<const char*>(module);

        char text[160];
        const auto written = fmt::format_to_n(text,
                                              sizeof(text),
                                              FMT_COMPILE("crash code={:#010x} at={}+{:#x} thread={}"),
                                              static_cast<uint32_t>(record.ExceptionCode),
                                              Word(name).data(),
                                              static_cast<uint64_t>(offset),
                                              GetCurrentThreadId());
        Write({ text, std::min<size_t>(written.size, sizeof(text)) });
        return previousCrashFilter ? previousCrashFilter(info) : EXCEPTION_CONTINUE_SEARCH;
    }

    // std::terminate, which is what an exception leaving a noexcept function or a
    // thread comes to. The crash filter never sees those.
    inline void __cdecl OnAbort(int)
    {
        char text[64];
        const auto written = fmt::format_to_n(text, sizeof(text), FMT_COMPILE("crash abort thread={}"), GetCurrentThreadId());
        Write({ text, std::min<size_t>(written.size, sizeof(text)) });
    }

    // Whether the log's last run says how it ended. What it can't say is a fail
    // fast, a kill or a power cut.
    inline bool SaidHowItEnded(const std::string_view log) noexcept
    {
        const auto start = log.rfind("] start ");
        if (start == std::string_view::npos)
        {
            return true;
        }
        const auto run = log.substr(start);
        return run.find("] exit\n") != std::string_view::npos ||
               run.find("] session ending\n") != std::string_view::npos ||
               run.find("] crash ") != std::string_view::npos;
    }

    // The log so far, moved aside first if it has grown past the limit.
    inline std::string ReadAndRoll(const wchar_t* const path)
    {
        auto log = til::io::read_file_as_utf8_string_if_exists(path);
        if (log.size() > rollAtBytes)
        {
            MoveFileExW(path, (std::wstring{ path } + L".old").c_str(), MOVEFILE_REPLACE_EXISTING);
        }
        return log;
    }

    // Once, by the process that owns the window.
    inline void Start() noexcept
    try
    {
        previousCrashFilter = SetUnhandledExceptionFilter(OnCrash);
        std::signal(SIGABRT, OnAbort);

        if (!SaidHowItEnded(ReadAndRoll(Path())))
        {
            Write("previous run ended without saying how");
        }

        std::array<wchar_t, PACKAGE_FULL_NAME_MAX_LENGTH + 1> package{};
        auto length = gsl::narrow_cast<UINT32>(package.size());
        const std::wstring_view name = GetCurrentPackageFullName(&length, package.data()) == ERROR_SUCCESS ? package.data() : L"none";
        Write(fmt::format(FMT_COMPILE("start package={}"), Word(name).data()));
    }
    CATCH_LOG()
}
