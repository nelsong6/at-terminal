// Win+` (the summon key) brings at-terminal forward on its own virtual desktop,
// then minimizes every other window there that can be minimized, so the desktop
// background shows through the translucent terminal (ATT-13). Only the summon
// key does this; launching at-terminal again or Alt+Tab from tmux only bring it
// forward.

#pragma once

namespace at_minimize_others
{
    // What the summon key reads about one top-level window.
    struct Window
    {
        bool ours = false; // belongs to at-terminal's own process
        bool visible = false;
        bool minimized = false;
        bool owned = false; // has an owner window, which minimizes it along with itself
        bool shell = false; // the desktop or the taskbar
        bool cloakedByApp = false; // hidden by its own program (a suspended Store app's frame), not by a desktop switch
        LONG style = 0;
        LONG exStyle = 0;
    };

    // A window is minimized when it is another program's visible, unminimized,
    // top-level window that offers a minimize button and is no tool window.
    // Whether it is on the terminal's desktop is asked separately, since that
    // is a cross-process call into Explorer.
    constexpr bool ShouldMinimize(const Window& w) noexcept
    {
        return !w.ours &&
               w.visible &&
               !w.minimized &&
               !w.owned &&
               !w.shell &&
               !w.cloakedByApp &&
               (w.style & WS_MINIMIZEBOX) != 0 &&
               (w.exStyle & WS_EX_TOOLWINDOW) == 0;
    }
}
