// at-terminal's off-screen window, for window-shot (at-home-tools) to photograph a
// tmux window Nelson isn't looking at (ATT-15, AHT-169):
//
//   at-terminal --offscreen <x>,<y>,<width>,<height> <commandline>
//
// starts a second at-terminal process, apart from the one with his window: it
// takes no part in the one-window handoff, registers no hotkeys, Alt+Tab hook,
// notification icon or default-terminal handoff, and saves no layout. Its one
// window runs the commandline full screen, as his does, but over the given
// rectangle (physical pixels), which window-shot places off every monitor; it is
// a tool window, so it has no taskbar button or Alt+Tab entry, and is never
// activated. Windows still composes it, so PrintWindow draws it as it would look.

#pragma once

namespace at_offscreen
{
    inline constexpr std::wstring_view flag{ L"--offscreen" };

    // The rectangle as "x,y,width,height": whole numbers, the size above zero.
    inline std::optional<RECT> ParseRect(const std::wstring_view text) noexcept
    {
        int values[4]{};
        size_t at = 0;
        for (int i = 0; i < 4; ++i)
        {
            const auto end = i < 3 ? text.find(L',', at) : text.size();
            if (end == std::wstring_view::npos || end == at)
            {
                return std::nullopt;
            }
            auto field = text.substr(at, end - at);
            const auto negative = field.front() == L'-';
            if (negative)
            {
                field.remove_prefix(1);
            }
            if (field.empty() || field.size() > 6)
            {
                return std::nullopt;
            }
            int value = 0;
            for (const auto c : field)
            {
                if (c < L'0' || c > L'9')
                {
                    return std::nullopt;
                }
                value = value * 10 + (c - L'0');
            }
            values[i] = negative ? -value : value;
            at = end + 1;
        }
        if (values[2] <= 0 || values[3] <= 0)
        {
            return std::nullopt;
        }
        return RECT{ values[0], values[1], values[0] + values[2], values[1] + values[3] };
    }

    enum class Parsed
    {
        NotOffscreen, // no --offscreen: the ordinary terminal
        Offscreen,
        Refused, // --offscreen with a bad rectangle or no commandline
    };

    // Whether args (the whole command line, program first) ask for the off-screen
    // window; if so, its rectangle and the command line that opens its tab.
    template<typename String>
    Parsed Parse(const std::vector<String>& args, RECT& rect, std::vector<String>& commandline)
    {
        if (args.size() < 2 || std::wstring_view{ args[1] } != flag)
        {
            return Parsed::NotOffscreen;
        }
        const auto parsed = args.size() > 3 ? ParseRect(std::wstring_view{ args[2] }) : std::nullopt;
        if (!parsed)
        {
            return Parsed::Refused;
        }
        rect = *parsed;
        commandline.clear();
        commandline.push_back(args[0]);
        // A new tab running exactly what follows, whatever it looks like.
        commandline.emplace_back(L"new-tab");
        commandline.emplace_back(L"--");
        commandline.insert(commandline.end(), args.begin() + 3, args.end());
        return Parsed::Offscreen;
    }

    // This process's rectangle, when it is the off-screen window. Set once, by the
    // window emperor, before any window exists.
    inline std::optional<RECT>& Window() noexcept
    {
        static std::optional<RECT> window;
        return window;
    }

    inline bool Active() noexcept
    {
        return Window().has_value();
    }
}
