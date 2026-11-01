# at-terminal

Nelson's build of Windows Terminal 1.25 (https://github.com/microsoft/terminal,
MIT), installed beside Microsoft's as **at-terminal** and launched by
`at-terminal`. tmux does all the multiplexing, so this terminal is only its
window (HOMECFG-60, https://openproject.romaine.life/wp/HOMECFG-60).

## What differs from Windows Terminal

- **One window, one tab, one pane.** Nothing opens another: new tab, split,
  duplicate, new window, move or drag a tab or pane out, the settings UI,
  an elevated window, a default-terminal handoff. The guard is
  `TerminalPage::_CanOpenPane`, and `WindowEmperor` refuses a second window.
- **Launching it again** (by any commandline) brings the open window forward
  and runs nothing in it.
- **Always full screen.** `GetLaunchMode` is always full screen, and nothing
  leaves it (F11 and Alt+Enter do nothing).
- **Its own package.** The Dev branding, renamed: identity `at-terminal`,
  publisher `CN=at-terminal`, alias `at-terminal.exe`, its own settings under
  `%LOCALAPPDATA%\Packages\at-terminal_<hash>\LocalState`. `wt.exe` stays
  Microsoft's.

## Build and release

`.github/workflows/at-terminal.yml` builds every PR and publishes each `main`
commit as a release `v1.25.<run>.0` holding the signed `.msix` and the
certificate (`at-terminal.cer`) that signs it. It runs on `windows-2025`,
which has Visual Studio 2026 (about 25 minutes). The signing certificate is
self-signed, in Key Vault `romaine-kv` (`at-terminal-signing-pfx`,
`at-terminal-signing-password`) and the repo secrets `AT_TERMINAL_PFX` and
`AT_TERMINAL_PFX_PASSWORD`.

This fork sends nothing upstream: no PRs or issues to microsoft/terminal
(its `AGENTS.md`).
