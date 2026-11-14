// Alt+Tab while at-terminal's window is in front is the terminal's key, sent on
// to the program in it (tmux reads it as M-Tab), not Windows' window switcher
// (TMUX-209). Windows acts on Alt+Tab before any window sees it, so a low-level
// keyboard hook takes it first. Everywhere else Alt+Tab stays Windows' own.

#pragma once

namespace at_alt_tab
{
    // Decides, for each Tab the hook sees, whether it is taken from Windows. A
    // Tab taken going down is taken coming up too, wherever focus and Alt are by
    // then, so the terminal never gets half a key and Windows never the other half.
    class Gate
    {
    public:
        bool Take(const DWORD vkey, const bool down, const bool alt, const bool ctrl, const bool win, const bool terminalInFront) noexcept
        {
            if (vkey != VK_TAB)
            {
                return false;
            }
            if (!down)
            {
                const auto taken = _taken;
                _taken = false;
                return taken;
            }
            // Ctrl+Alt+Tab (the switcher that stays open) and Win+Alt+Tab stay Windows'.
            _taken = alt && !ctrl && !win && terminalInFront;
            return _taken;
        }

    private:
        bool _taken = false;
    };
}
