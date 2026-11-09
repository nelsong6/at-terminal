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
- **Launching it comes to the front.** When Windows refuses the first window
  the foreground, as it does a program started by a background process, the
  window takes it the way the summon key does.
- **Always full screen.** `GetLaunchMode` is always full screen, and nothing
  leaves it (F11 and Alt+Enter do nothing). It covers the whole screen even
  with an auto-hide taskbar: stock Windows Terminal leaves 2px unpainted at
  that edge for revealing the taskbar, which full screen doesn't do (ATT-5).
- **Its own package.** The Dev branding, renamed: identity `at-terminal`,
  publisher `CN=at-terminal`, alias `at-terminal.exe`, its own settings under
  `%LOCALAPPDATA%\Packages\at-terminal_<hash>\LocalState`. `wt.exe` stays
  Microsoft's.
- **Its text zoom lasts.** The zoom is saved in `state.json` (`fontSizeDelta`) when
  it changes and reapplied before the first layout, so a restart keeps it.
- **A bad state file doesn't stop it opening.** Stock Windows Terminal crashes on
  launch when any value in `state.json` can't be read. Here that value keeps its
  default and the rest of the file loads. A file with a problem is copied to
  `state.json.bad` and rewritten without what was ignored, and the first window
  lists the problems in a dialog. Tests: `ApplicationStateTests`, run by CI.

- **No "Termination behavior" bar.** Stock Windows Terminal shows an info bar with
  "Don't show again" when a pane's program fails. The bar, its strings and the
  `dismissedMessages` field in `state.json` are gone; a file still holding that
  field loads without a problem. The exit message in the pane stays.

- **No scrollbar.** tmux keeps the history, so the terminal's own scrollbar only
  ever showed a full track, and it took a column from the text. It is gone, with
  its settings (`scrollbarState`, `showMarksOnScrollbar`) and their settings-page
  entries; a settings file still holding them loads and leaves them out when
  written. The `ScrollBar` stays in `TermControl.xaml`, collapsed, as the scroll
  position the control's scrolling code reads and sets. Test:
  `RemovedScrollbarSettingsAreDropped`, run by CI.

- **It keeps a log.** `at-terminal.log` in `LocalState`, one of the home logs
  (`appLogs` in the home profile's `machine/state.json`): start and exit, the tmux
  server it connected to, that connection failing or ending and how, and crashes.
  Lines hold fixed words, numbers and program names, never anything from the
  terminal; `AtLogTests` holds them to that, and all of it lives in
  `src/cascadia/inc/AtLog.h`. A fail fast or a kill leaves no line of its own, so
  the next start says the previous run ended without saying how.

## Build and release

`.forgejo/workflows/at-terminal.yml` builds every PR and releases each `main`
commit as `v1.25.<run>.0`, holding the signed `.msix` and the certificate
(`at-terminal.cer`) that signs it. The release is a draft until pipeline-templates'
`home-sync.yml` publishes it and starts home-sync on every PC, which installs the
newest release (AHT-86), so each merge reaches the PCs within minutes, behind the
open window's 30-second countdown. It runs on upstairspc in one kept
folder, `%USERPROFILE%\at-terminal-ci\tree`, so only what changed recompiles;
deleting that folder forces a full build (about 12 minutes). Restore, build and
tests run in a Windows container with that folder mounted, from the image in
`.forgejo/container` (about 15 minutes to build, only when its files change, and
25 GB). Signing stays on the host. The signing certificate is
self-signed, in Key Vault `romaine-kv` (`at-terminal-signing-pfx`,
`at-terminal-signing-password`) and the repo secrets `AT_TERMINAL_PFX` and
`AT_TERMINAL_PFX_PASSWORD`.

This fork sends nothing upstream: no PRs or issues to microsoft/terminal
(its `AGENTS.md`).
