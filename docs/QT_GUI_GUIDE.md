# Qt GUI Guide

A walkthrough of `app_top_monitoring_qt`'s windows, menus, and dialogs. For the underlying architecture, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Main window

The main window shows the live local process table, refreshing on a timer (default every 1000 ms, adjustable via the **Interval** spin box in the Filters & Sorting row). The window title always includes the current version (`NEO Monitoring Services v1.0.0`).

**Toolbar:**

| Button | Action |
| --- | --- |
| Refresh | Force an immediate refresh |
| Pause / Resume | Stop/resume the refresh timer |
| Settings | Opens the [Settings dialog](#settings-dialog) |
| Connect to Remote | Opens the [Connect to Remote dialog](#connect-to-remote-dialog) |
| 🌙 Dark Mode / ☀ Light Mode | Toggles the theme; the choice persists between sessions |

**Filters & Sorting row:** a search box (matches command name/line, like the CLI's include pattern), a state filter dropdown, a sort-mode dropdown, a tree-view checkbox (reserved for future use), and the refresh interval.

**Status bar:** shows the last refresh time normally; while viewing remote data, shows a **Back to Local** button instead (see below) — deliberately placed in the status bar rather than the toolbar, since a toolbar with too many buttons can silently push extras into an overflow area where they become unclickable.

**Menu bar:**
- **View** → Live Graphs..., Capture..., Logs..., Alert Thresholds...
- **Help** → About NEO Monitoring Services

**Process table:** right-click a row for a context menu with **Send SIGTERM**, **Send SIGKILL** (both ask for confirmation first) and **Renice...** (prompts for a new niceness, -20 to 19). Local processes only — while viewing a remote host's table (see [Connect to Remote](#connect-to-remote-dialog)), the menu shows a single disabled entry instead, since acting on a process over a remote monitoring connection is a different trust decision than merely observing it.

**System tray:** if a tray is available, the app adds an icon showing a live CPU/Memory tooltip. Closing the main window (or minimizing it) hides it to the tray instead of exiting; use the tray icon's **Show/Hide** or double/single-click to bring it back, and **Quit** from the tray menu to actually exit. Useful if you leave the app running in the background continuously.

## Settings dialog

Mirrors the CLI's filter/sort/display flags as editable fields: refresh interval, CPU/RAM thresholds, include/exclude patterns, user/state/PID/PPID filters, sort mode, process limit, reverse sort, and the display toggles (long args, VSZ, swap, I/O, threads, start time, elapsed). Opens pre-filled with the live configuration.

## Connect to Remote dialog

Covers all four remote protocols from one window (non-modal — you can keep monitoring locally while it's open):

- **Saved target** dropdown at the top lists connection profiles saved to `~/.config/neo-monitoring/targets.json` — the same file the CLI's `--target`/`--save-target`/`--list-targets`/`--delete-target` flags use, so a target saved from one is usable from the other. Picking one fills in every field below (protocol, host, port, and the login or deploy fields as appropriate) in one click, instead of starting from blank fields every time.
- **Save...** prompts for a name (pre-filled with the currently-selected target's name, so re-saving over it is one click) and stores everything currently in the form under that name, asking for confirmation before overwriting an existing one.
- **Delete** removes the currently-selected saved target (with a confirmation prompt) and refreshes the dropdown back to "(none)".
- **Protocol** dropdown switches between a monitor-fields page (SSH/Telnet: user, password, SSH key browse, remote binary name) and a deploy-fields page (FTP/TFTP: user/password for FTP, local file browse, remote filename), and updates the default port automatically. Loading a saved target selects the matching protocol first.
- **Connect && Fetch** (SSH/Telnet) runs on a background thread; on success, it **replaces the main table's contents** with the card's process list and pauses local polling. A **Back to Local** control appears in the main window's status bar to return.
- **Deploy** (FTP/TFTP) uploads the selected file and logs the result.
- A log pane at the bottom timestamps every action and result.

Saved targets are stored in plain text (a Telnet/FTP password included, if you saved one) with the file restricted to your own user (`0600`) — SSH targets never save a password, since SSH here is always key-based.

See [REMOTE_TESTING.md](REMOTE_TESTING.md) for setting up local test servers to try this without real hardware.

## Live Graphs

**View → Live Graphs...** opens an always-on rolling view: four native, `QPainter`-drawn line graphs (CPU, Memory, Swap, I/O Read+Write) fed one sample per local refresh tick.

- **Show** dropdown: all four graphs at once, or isolate one.
- **Clear** resets all four graphs' history.
- Rolling 120-sample window (~2 minutes at the default 1 s interval) — old samples are discarded, since this view is meant to run indefinitely.
- X axis shows relative recency (no time labels); Y axis is fixed 0–100% for CPU/Memory/Swap and auto-scaled for I/O.
- Colors follow the app's theme automatically.
- Paused while viewing remote data; resumes with **Back to Local**.

No export from this window — it's a live view, not a saved session. For that, use Capture.

## Logs

**View → Logs...** shows the same log stream the CLI writes to `--log-file` (refresh errors, remote connection attempts/failures, capture start/stop, process actions, alert firings), live, inside the Qt app — including messages produced by background threads such as the SSH/FTP/TFTP work in the Connect to Remote dialog, which would otherwise only be visible in that dialog's own log pane while it happens to be open.

- A **minimum level** dropdown filters what's shown (the full history is kept regardless, so raising the filter never loses anything).
- **Auto-scroll** keeps the newest entry in view; uncheck it to read back through history without it jumping.
- **Clear** empties this window's view (does not touch `--log-file`, if one is configured).
- The window can be closed and reopened freely — messages logged while it's closed are not lost, and are replayed on reopen.

## Alert Thresholds

**View → Alert Thresholds...** configures the same sustained CPU%/memory% threshold alerting as the CLI's `--alert-*` flags, evaluated against the local machine every refresh regardless of which process list (local or remote) is currently displayed:

- **CPU threshold** / **Memory threshold** (percent; `0`/"Off" disables that check).
- **Must persist for** (seconds) — a threshold has to stay exceeded for this long before it fires, so a brief spike doesn't trigger anything.
- **Desktop notification** shows a system tray balloon when an alert fires (requires the [system tray icon](#main-window) to be available).
- **Webhook URL** (optional) POSTs a small JSON payload when an alert fires, fired off in the background so a slow endpoint never blocks the UI.

Changing any of these resets the sustained-breach tracking, so a new configuration always starts from a clean slate.

## Capture

**View → Capture...** is the Qt equivalent of the CLI's `--capture`: a bounded, exportable session rather than an always-on view.

- **Duration** (seconds; `0` runs until you click **Stop** — same semantics as the CLI's bare `--capture` vs `--capture=N`).
- **Start Capture** / **Stop** control the session; the status line shows elapsed time and sample count live.
- Unlike Live Graphs, there's **no rolling cap** — the entire session is kept, matching what `--capture` would have recorded.
- **X axis shows real wall-clock time** (`HH:mm:ss`), not elapsed seconds or relative recency — useful for correlating a spike with something you know happened at a specific time.
- **Save Report...** exports the exact same CSV/HTML pair the CLI's `--capture` produces (same `monitoring_capture` functions under the hood); pick a base filename and both `.csv` and `.html` are written next to it.
- Same **Show** dropdown as Live Graphs.

## About dialog

**Help → About NEO Monitoring Services** shows the app name, current version (from the `VERSION` file), a one-line description, and the Qt version it was built with.
