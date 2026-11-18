// The Win key pressed and released on its own does what Win+` does: summons
// at-terminal (ATT-16). Win with any other key is Windows' as before, and
// Ctrl+Esc or the Start button opens Start.
//
// Windows opens Start on a Win release with no other key since its press, so
// when a press could be a tap the hook sends one key Windows has no use for
// (MaskKey) while Win is still down, the way AutoHotkey keeps Start shut. The
// real Win press and release go through untouched, so Windows never sees Win
// stuck down, whatever happens to the hook.

#pragma once

namespace at_win_tap
{
    // Unassigned in Windows' virtual-key table (AutoHotkey's default mask key).
    inline constexpr WORD MaskKey = 0xE8;

    // Win+` alone, nothing else held: the chord whose summon the tap repeats.
    inline bool IsWinBacktick(const bool ctrl, const bool alt, const bool shift, const bool win, const int32_t vkey) noexcept
    {
        return win && !ctrl && !alt && !shift && vkey == VK_OEM_3;
    }

    enum class Act
    {
        None,
        Mask, // a Win press that could be a tap: send MaskKey so Start stays shut
        Summon, // the release of a tap
    };

    // Fed every key the hook sees except its own MaskKey.
    class Gate
    {
    public:
        // wasDown: the key was already held before this event (a held key's
        // repeat). modifierHeld: Ctrl, Alt or Shift is down.
        Act Key(const DWORD vkey, const bool down, const bool wasDown, const bool modifierHeld) noexcept
        {
            const auto isWin = vkey == VK_LWIN || vkey == VK_RWIN;
            if (!isWin)
            {
                if (down)
                {
                    _armed = false;
                }
                return Act::None;
            }
            if (down)
            {
                if (vkey == _held && wasDown)
                {
                    return Act::None;
                }
                if (_held != 0 && _held != vkey)
                {
                    // The other Win key while one is held: a chord, not a tap.
                    // (A release Windows kept from the hook costs one tap here;
                    // the next release of either Win key clears it.)
                    _armed = false;
                    return Act::None;
                }
                // A fresh press, or a press after a release Windows kept from
                // the hook (the lock screen's, say).
                _held = vkey;
                _armed = !modifierHeld;
                return _armed ? Act::Mask : Act::None;
            }
            const auto tap = _armed && vkey == _held;
            _armed = false;
            _held = 0;
            return tap ? Act::Summon : Act::None;
        }

    private:
        DWORD _held = 0;
        bool _armed = false;
    };
}
