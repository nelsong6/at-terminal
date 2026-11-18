// The Win key pressed and released on its own is Win+` (ATT-16): at-terminal's
// summon from another app, and the at menu (at-tmux's binding) with the
// terminal already in front. Win with any other key is Windows' as before, and
// Ctrl+Esc or the Start button opens Start.
//
// The hook holds back the release of a tap and sends, in its place, a press and
// release of ` and then the release, so whatever answers Win+` sees one, and
// Windows, which opens Start on a Win release with no other key since its
// press, keeps Start shut. It can't send a key at the press instead: a key sent
// from inside the hook is processed before the press the hook is deciding on,
// so Windows still sees Win alone (tested on upstairspc, ATT-16).

#pragma once

namespace at_win_tap
{
    // The ` key the tap sends: its US-layout virtual key and its scan code,
    // the key left of 1 on any layout (Windows Terminal's default Win+sc(41)).
    inline constexpr WORD BacktickVkey = VK_OEM_3;
    inline constexpr WORD BacktickScanCode = 0x29;

    // Win+` alone, nothing else held: the tap is on while a summon hotkey has this chord.
    inline bool IsWinBacktick(const bool ctrl, const bool alt, const bool shift, const bool win, const int32_t vkey) noexcept
    {
        return win && !ctrl && !alt && !shift && vkey == VK_OEM_3;
    }

    // Fed every key the hook sees except the keys it sends itself.
    class Gate
    {
    public:
        // True for the release that ends a tap. wasDown: the key was already
        // held before this event (a held key's repeat). modifierHeld: Ctrl, Alt
        // or Shift is down.
        bool Tap(const DWORD vkey, const bool down, const bool wasDown, const bool modifierHeld) noexcept
        {
            const auto isWin = vkey == VK_LWIN || vkey == VK_RWIN;
            if (!isWin)
            {
                if (down)
                {
                    _armed = false;
                }
                return false;
            }
            if (down)
            {
                if (vkey == _held && wasDown)
                {
                    return false;
                }
                if (_held != 0 && _held != vkey)
                {
                    // The other Win key while one is held: a chord, not a tap.
                    // (A release Windows kept from the hook costs one tap here;
                    // the next release of either Win key clears it.)
                    _armed = false;
                    return false;
                }
                // A fresh press, or a press after a release Windows kept from
                // the hook (the lock screen's, say).
                _held = vkey;
                _armed = !modifierHeld;
                return false;
            }
            const auto tap = _armed && vkey == _held;
            _armed = false;
            _held = 0;
            return tap;
        }

    private:
        DWORD _held = 0;
        bool _armed = false;
    };
}
