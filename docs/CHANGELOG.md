# Changelog

All notable changes to this project are documented here. Format loosely follows [Keep a Changelog](https://keepachangelog.com/), and the version number matches the `VERSION` file at the project root (see [ARCHITECTURE.md](ARCHITECTURE.md#version-management)).

## [1.0.0]

### Added

**Core monitoring**
- Linux `/proc` process scanning: PID, PPID, user, state, threads, CPU%, memory%, RSS, VSZ, swap, I/O read/write rate, start time, elapsed runtime, command name, and full command line.
- Filtering by command pattern, exclude pattern, PID, PPID, user/UID, state, CPU threshold, RAM threshold (MB and %).
- Sorting by CPU, memory, PID, RSS, I/O read, or I/O write, with a reverse option.
- Table, CSV, and JSON output modes; one-shot and batch execution.
- Interactive terminal controls (`q`/`c`/`m`/`p`/`+`/`-`/`r`/`v`).

**Remote connectivity**
- `--protocol ssh|telnet` process monitoring: fetches a remote card's process list and system totals (CPU count, memory) in a single round trip, applying local filters/sorting to the result.
- `--protocol ftp|tftp` deployment: uploads a file (typically the CLI binary itself) to a card.
- Shells out to standard client tools (`ssh`, `telnet`, `curl`, `tftp`) rather than linking networking libraries.

**Capture and graphing**
- `--capture`/`--capture=SECONDS`: records system-wide CPU/memory/swap/I-O over time, writing a CSV and a self-contained interactive HTML/Chart.js report with a graph-selector dropdown.
- Qt native equivalents: an always-on **Live Graphs** window (`QPainter`-drawn, no QtCharts dependency) and a bounded, exportable **Capture** window with a real-clock-time X axis.

**Qt desktop application**
- Full process table with live updates, matching the CLI's filter/sort/display options via a Settings dialog.
- Light/dark theme, persisted between sessions.
- **Connect to Remote** dialog covering all four protocols, with background-thread networking so the UI never blocks.
- Menu bar with **View** (Live Graphs, Capture) and **Help** (About) menus.

**Build and tooling**
- CMake build with an optional `BUILD_QT_APP` flag, so the CLI builds and runs without Qt installed at all.
- VS Code tasks for building, running, and exercising every mode (local, SSH, Telnet, FTP, TFTP, capture).
- A staged GitHub Actions pipeline: dependency verification, parallel CLI/Qt builds, then publish.
- Version management via a single `VERSION` file, propagated through CMake into both binaries and displayed by `--version` (CLI) and the title bar/About dialog (Qt).

**Documentation**
- README with feature overview, build/run instructions, and CLI examples.
- `docs/ARCHITECTURE.md`, `docs/CLI_REFERENCE.md`, `docs/QT_GUI_GUIDE.md` (this directory).
- `docs/REMOTE_TESTING.md`: setting up local SSH/Telnet/FTP/TFTP test servers.

### Known limitations

- `--capture` and the Qt Capture/Live Graphs windows cover local monitoring only; not yet combined with `--protocol`.
- Telnet automation is best-effort (scripted login, no terminal negotiation) — prefer SSH where the card supports it.
- The HTML capture report requires internet access to view (Chart.js loads from a CDN); the Qt native graphs do not.
