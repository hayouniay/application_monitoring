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
- **View** → Live Graphs..., Capture...
- **Help** → About NEO Monitoring Services

## Settings dialog

Mirrors the CLI's filter/sort/display flags as editable fields: refresh interval, CPU/RAM thresholds, include/exclude patterns, user/state/PID/PPID filters, sort mode, process limit, reverse sort, and the display toggles (long args, VSZ, swap, I/O, threads, start time, elapsed). Opens pre-filled with the live configuration.

## Connect to Remote dialog

Covers all four remote protocols from one window (non-modal — you can keep monitoring locally while it's open):

- **Protocol** dropdown switches between a monitor-fields page (SSH/Telnet: user, password, SSH key browse, remote binary name) and a deploy-fields page (FTP/TFTP: user/password for FTP, local file browse, remote filename), and updates the default port automatically.
- **Connect && Fetch** (SSH/Telnet) runs on a background thread; on success, it **replaces the main table's contents** with the card's process list and pauses local polling. A **Back to Local** control appears in the main window's status bar to return.
- **Deploy** (FTP/TFTP) uploads the selected file and logs the result.
- A log pane at the bottom timestamps every action and result.

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
