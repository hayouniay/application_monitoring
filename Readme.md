# NEO Monitoring Services

A professional Linux process-monitoring application inspired by `top` — with the ability to reach out and monitor or deploy to a remote card over SSH, Telnet, FTP, or TFTP.

NEO Monitoring Services provides:

* A lightweight terminal-based process monitor.
* A Qt 6 desktop application with light/dark theming.
* Linux `/proc` process inspection.
* CPU, memory, RSS, VSZ, swap, and I/O monitoring.
* Process filtering and sorting.
* Configurable refresh intervals.
* Interactive monitoring controls.
* CSV and JSON output modes.
* Remote process monitoring over SSH and Telnet.
* Remote binary deployment over FTP and TFTP.
* A reusable C monitoring backend shared by the CLI and Qt application.
* A staged GitHub Actions pipeline for dependency checks, parallel CLI/Qt builds, and publishing.

## Documentation

This README covers installation, building, and everyday usage. For deeper reference material, see [`docs/`](docs/):

| Document | Covers |
| --- | --- |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module responsibilities, data flow, threading model, version management internals |
| [CLI_REFERENCE.md](docs/CLI_REFERENCE.md) | Every CLI flag, exhaustively, grouped by purpose |
| [QT_GUI_GUIDE.md](docs/QT_GUI_GUIDE.md) | A walkthrough of every Qt window, menu, and dialog |
| [REMOTE_TESTING.md](docs/REMOTE_TESTING.md) | Setting up local SSH/Telnet/FTP/TFTP test servers |
| [CHANGELOG.md](docs/CHANGELOG.md) | Version history |

## Features

### Process Monitoring

The application collects information directly from Linux `/proc`, including:

* PID
* PPID
* User
* Process state
* Thread count
* CPU usage
* Memory usage
* RSS
* VSZ
* Swap
* Disk read/write counters
* Disk read/write rates
* Process start time
* Elapsed runtime
* Command name
* Full command line

### Filtering

Processes can be filtered by:

* Command name
* Full command line
* PID
* PPID
* User
* UID
* Process state
* CPU threshold
* RAM threshold
* RAM percentage threshold
* Exclusion patterns

Pattern matching is case-insensitive substring matching against the process command name and command line. Local filters are also applied client-side to processes fetched from a remote card over SSH/Telnet.

### Sorting

Available sorting modes include:

* CPU
* Memory
* PID
* RSS
* I/O read
* I/O write

Sorting can also be reversed.

### Remote Connectivity

NEO can reach a remote card in two ways:

* **Monitor** a card's process list over **SSH** (key-based) or **Telnet** (scripted login), fetched in a single round trip alongside the card's total memory and CPU count.
* **Deploy** the `app_top_monitoring` binary (or any file) to a card over **FTP** or **TFTP**, so it can then be monitored remotely.

This works from both the CLI (`--protocol ssh|telnet|ftp|tftp`) and the Qt app (**Connect to Remote** dialog). See [`docs/REMOTE_TESTING.md`](docs/REMOTE_TESTING.md) for how to set up local test servers for each protocol without needing real hardware.

Remote monitoring runs on top of the same backend as local monitoring: the remote card runs its own `app_top_monitoring --csv --once`, and NEO parses the result, applies local filters/sorting, and displays it exactly like local data.

**SSH/Telnet monitoring flags:**

| Flag | Description |
| --- | --- |
| `--protocol ssh\|telnet` | Selects the remote protocol |
| `--host HOST` | Remote card address |
| `-u, --user USER` | Login username |
| `--password PASS` | Login password (Telnet only; SSH uses keys) |
| `--port N` | Override the default port (22 for SSH, 23 for Telnet) |
| `--identity KEYFILE` | SSH private key (optional) |
| `--remote-bin NAME` | Name/path of this tool on the card (default `app_top_monitoring`) |

**FTP/TFTP deploy flags:**

| Flag | Description |
| --- | --- |
| `--protocol ftp\|tftp` | Selects the deploy protocol |
| `--host HOST` | Remote card address |
| `-u, --user USER` | FTP username (omit for anonymous; ignored by TFTP, which has no auth) |
| `--password PASS` | FTP password |
| `--port N` | Override the default port (21 for FTP, 69 for TFTP) |
| `--local-file PATH` | File to upload |
| `--remote-file NAME` | Destination filename (defaults to the local filename) |

All existing filtering, sorting, and output flags (`--csv`, `--json`, `--sort`, `--limit`, state/pattern filters, etc.) continue to apply on top of `--protocol ssh`/`--protocol telnet` results.

**Runtime dependencies** for remote features (client tools spawned as subprocesses, not linked libraries): `openssh-client`, `telnet`, `curl` (used for FTP), and `tftp-hpa`. The card itself needs the matching server (`sshd`, `telnetd`, an FTP server, or a TFTP server).

### Capture and Graphing

`--capture` records system-wide metrics over time and produces a chart report you can open in a browser — useful for spotting trends (a slow memory leak, a periodic CPU spike) that a live table doesn't make obvious.

| Flag | Description |
| --- | --- |
| `--capture` | Capture until you press **Ctrl+C** |
| `--capture=SECONDS` | Capture for `SECONDS` then stop automatically — note the `=`, required by `getopt` for an optional argument; `--capture 30` (space-separated) does **not** work as a duration (`30` would instead be read as a search pattern) |
| `--capture-output PATH` | Base path for the output files, no extension (default: `capture_<unix-timestamp>` in the current directory) |

Each sample records: system-wide CPU utilization, memory utilization, swap utilization (all computed directly from `/proc/stat`/`/proc/meminfo`, independent of per-process figures), and the aggregate I/O rate and count of whatever processes match the active filters at that moment. A capture session honors the same `--interval`, filter, and sort flags as normal monitoring.

Two files are written when the capture ends:

* **`PATH.csv`** — the raw samples, for your own analysis.
* **`PATH.html`** — a self-contained interactive report (uses [Chart.js](https://www.chartjs.org/) from a CDN, so viewing it requires internet access): one line chart per metric (CPU, Memory, Swap, I/O) in a single page, each with a legend, plus a dropdown to show all graphs at once or isolate just one.

```bash
# Capture for 30 seconds, then open the report
./build/app_top_monitoring --capture=30 --interval 1
xdg-open capture_*.html   # or just double-click it

# Capture indefinitely until Ctrl+C, with a fixed output name
./build/app_top_monitoring --capture --capture-output myrun
```

Currently local monitoring only — combining `--capture` with `--protocol ssh|telnet|ftp|tftp` is rejected with a clear error rather than silently doing the wrong thing.

The Qt app has its own native equivalent — see **Live Graphs** below — rather than generating an HTML file.

### Live Graphs (Qt)

**View → Live Graphs...** opens a window with four native, `QPainter`-drawn line graphs — CPU, Memory, Swap, and I/O (Read/Write) — fed one sample at a time as local monitoring refreshes, using the same system-wide `/proc` readers as `--capture`. No new Qt module or external dependency (no QtCharts, no internet access needed, unlike the HTML report's CDN-hosted Chart.js).

* A dropdown lets you show all four graphs at once or isolate a single one — the native equivalent of the HTML report's toggle.
* Each graph shows a live legend (current value) in the series' color, gridlines with axis labels, and a soft gradient fill under single-series curves.
* CPU/Memory/Swap use a fixed 0–100% axis; I/O auto-scales since it's unbounded.
* Colors follow the app's light/dark theme automatically.
* **Clear** resets all four graphs' history.

Paused while viewing remote data (mirrors the process table); resumes when you go **Back to Local**.

### Capture (Qt)

**View → Capture...** is the Qt equivalent of the CLI's `--capture`: records CPU/Memory/Swap/I-O over a chosen duration (or until you click Stop), using the same four native wave graphs as Live Graphs — but with two key differences suited to a bounded, exportable session rather than an always-on rolling view:

* **The X axis shows real wall-clock time** (`HH:mm:ss`) rather than elapsed seconds — so you can tell exactly *when* a spike happened, not just how long ago.
* **No rolling cap** — the full session is kept (matching the CLI's behavior), so the exported report reflects everything captured, not just the last couple of minutes.

Set **Duration** (seconds; `0` runs until you click **Stop**), click **Start Capture**, and watch the graphs build live. **Save Report...** exports the exact same CSV/HTML pair `--capture` produces on the CLI, via the shared backend — pick a base filename and both `.csv` and `.html` are written next to it. The **Show** dropdown offers the same all-graphs/one-graph toggle as Live Graphs.

### CLI Interactive Controls

The terminal monitor provides interactive controls including:

| Key | Action                    |
| --- | ------------------------- |
| `q` | Quit                      |
| `c` | Sort by CPU               |
| `m` | Sort by memory            |
| `p` | Sort by PID               |
| `+` | Increase refresh interval |
| `-` | Decrease refresh interval |
| `r` | Refresh                   |
| `v` | Reverse sorting           |

### Output Modes

The CLI supports:

* Interactive table output
* One-shot output
* Batch mode
* CSV
* JSON
* Color control
* Process limits

### Qt Desktop Application

The Qt application provides a graphical interface with:

* Process table
* Live process updates
* Refresh and pause controls
* Search filtering
* State filtering
* User filtering
* PID filtering
* CPU threshold
* RAM threshold
* Refresh interval
* Sorting controls
* Process details panel
* Advanced settings dialog
* Display configuration
* Reverse sorting
* Process limit
* Include/exclude patterns
* Light/dark theme toggle, persisted between sessions
* **Connect to Remote** dialog: SSH/Telnet monitoring and FTP/TFTP deploy, all four protocols in one place
* **Live Graphs** window: native CPU/Memory/Swap/I-O line graphs, no browser or extra Qt module required
* **Capture** window: bounded, exportable capture session with a real wall-clock X axis, mirroring `--capture`
* **Help → About** dialog showing the current version
* A successful remote fetch replaces the table's contents with the card's process list; a **Back to Local** control (status bar) returns to live local monitoring

The Qt interface uses the same C monitoring backend as the CLI. Remote network calls run on a background thread so the UI never blocks.

## Architecture

The project is divided into a reusable C backend and two frontends.

```text
                    ┌─────────────────────────┐
                    │   NEO Monitoring Core   │
                    │                         │
                    │       Linux /proc       │
                    │           │             │
                    │    Process Scanner      │
                    │    Metrics Engine       │
                    │    Filter Engine        │
                    │    Output Engine        │
                    │    Remote Engine        │
                    │  (SSH/Telnet/FTP/TFTP)  │
                    └────────────┬────────────┘
                                 │
                    ┌────────────┴────────────┐
                    │                         │
                    ▼                         ▼
             CLI Application          Qt Desktop Application
                    │                         │
                    ▼                         ▼
             Terminal UI               Qt Widgets UI
```

The shared backend is compiled as:

```text
monitoring_core
```

and linked by both executables. The Remote Engine (`monitoring_remote.c`) shells out to standard client tools (`ssh`, `telnet`, `curl`, `tftp`) via a small process-spawning harness — no new libraries are linked into the binary, only optional runtime dependencies.

## Qt Monitoring Flow

The desktop application follows this update flow for local monitoring:

```text
QTimer
   │
   ▼
refresh()
   │
   ├── read_system_info()
   │
   ├── scan_processes()
   │
   ├── sort_processes()
   │
   ▼
processesUpdated()
   │
   ▼
NeoQtProcessModel
   │
   ▼
QTableView
```

Remote monitoring follows a parallel path that feeds the same model:

```text
"Connect to Remote" dialog
   │
   ▼
background thread
   │
   ├── remote_ssh_scan() / remote_telnet_scan()
   │       (spawns ssh/telnet, parses CSV + system info)
   │
   ▼
processesFetched()  [marshaled back to the UI thread]
   │
   ▼
NeoQtMainWindow::showRemoteProcesses()
   │       (pauses local polling)
   │
   ▼
NeoQtProcessModel
   │
   ▼
QTableView
```

This keeps Linux process collection, metric calculation, and remote transport inside the C backend while the Qt layer handles presentation, threading, and user interaction.

## Project Structure

```text
app_top_monitoring/
├── CMakeLists.txt
├── Makefile
├── README.md
├── VERSION
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── CLI_REFERENCE.md
│   ├── QT_GUI_GUIDE.md
│   ├── CHANGELOG.md
│   └── REMOTE_TESTING.md
│
├── .github/
│   └── workflows/
│       ├── pipeline.yml
│       ├── stage-1-verify-dependencies.yml
│       ├── stage-2-build-qt.yml
│       ├── stage-3-build-cli.yml
│       └── stage-4-publish.yml
│
├── .vscode/
│   └── tasks.json
│
├── inc/
│   ├── monitoring_services.h
│   ├── monitoring_process.h
│   ├── monitoring_metrics.h
│   ├── monitoring_filter.h
│   ├── monitoring_ui.h
│   ├── monitoring_output.h
│   ├── monitoring_remote.h
│   ├── monitoring_capture.h
│   ├── monitoring_version.h.in
│   │
│   └── qt/
│       ├── qt_application.h
│       ├── qt_main_window.h
│       ├── qt_process_model.h
│       ├── qt_monitor_controller.h
│       ├── qt_settings_dialog.h
│       ├── qt_remote_dialog.h
│       ├── qt_theme.h
│       ├── qt_wave_widget.h
│       ├── qt_graphs_window.h
│       └── qt_capture_window.h
│
├── src/
│   ├── monotoring_services.c
│   ├── monitoring_services_core.c
│   ├── monitoring_process.c
│   ├── monitoring_metrics.c
│   ├── monitoring_filter.c
│   ├── monitoring_ui.c
│   ├── monitoring_output.c
│   ├── monitoring_remote.c
│   ├── monitoring_capture.c
│   │
│   └── qt/
│       ├── qt_application.cpp
│       ├── qt_main_window.cpp
│       ├── qt_process_model.cpp
│       ├── qt_monitor_controller.cpp
│       ├── qt_settings_dialog.cpp
│       ├── qt_remote_dialog.cpp
│       ├── qt_theme.cpp
│       ├── qt_wave_widget.cpp
│       ├── qt_graphs_window.cpp
│       └── qt_capture_window.cpp
│
└── ui/
    └── qt/
        └── resources/
            └── monitoring.qrc
```

## Requirements

### Linux

NEO Monitoring Services is designed for Linux systems with a `/proc` filesystem.

Required development tools:

* GCC or another C11-compatible compiler
* CMake 3.16+
* Make
* Linux `/proc`
* POSIX threads (`pthread`, via CMake's `Threads` package — used by the Qt app's background network calls)

### Qt

The desktop application requires:

* Qt 6
* Qt Widgets
* A C++17-compatible compiler

The CLI target does **not** require Qt at all — see [Building the CLI without Qt](#building-the-cli-without-qt) below.

The Qt application is built without Qt Designer. The interface is currently constructed programmatically using Qt Widgets.

### Remote features (optional, runtime only)

Only needed if you use `--protocol ssh|telnet|ftp|tftp` or the Qt "Connect to Remote" dialog. Not required to build or run local monitoring.

```bash
sudo apt install openssh-client telnet curl tftp-hpa
```

The remote card needs the matching **server**: `sshd`, `telnetd`, an FTP server (e.g. `vsftpd`), or a TFTP server (e.g. `tftpd-hpa`). See [`docs/REMOTE_TESTING.md`](docs/REMOTE_TESTING.md) to set these up locally for testing without real hardware.

## Building

### Build everything

```bash
make
```

or:

```bash
cmake -S . -B build
cmake --build build
```

### Build the CLI

```bash
make cli
```

### Build the Qt application

```bash
make qt
```

### Building the CLI without Qt

The CLI target links only `monitoring_core` and never touches Qt, so it can be configured and built on a machine without Qt6 installed at all:

```bash
cmake -S . -B build -DBUILD_QT_APP=OFF
cmake --build build --target app_top_monitoring
```

`make cli` already passes `-DBUILD_QT_APP=OFF` for you.

### Debug build

```bash
make debug
```

### Release build

```bash
make release
```

### Clean

```bash
make clean
```

## Continuous Integration

The project ships a staged GitHub Actions pipeline (`.github/workflows/`), split into one reusable workflow file per stage:

```text
stage 1: verify-dependencies
   │
   ├──► stage 2: build-qt   ──┐
   │                          ├──► stage 4: publish
   └──► stage 3: build-cli  ──┘
```

* **Stage 1** installs the toolchain and confirms `cmake`/`g++`/Qt6 resolve, and that `Makefile`/`CMakeLists.txt` exist.
* **Stages 2 and 3** run in parallel, each depending only on Stage 1: `make qt` and `make cli` respectively, each uploading its binary as a build artifact.
* **Stage 4** waits on both, downloads both binaries, and publishes a combined release artifact (optionally attached to a GitHub Release on version tags).

`pipeline.yml` is the only workflow that triggers on push/PR; it calls the four stage files via `workflow_call`.

## Running

### CLI

```bash
./build/app_top_monitoring
```

Display help:

```bash
./build/app_top_monitoring --help
```

### Qt Application

```bash
./build/app_top_monitoring_qt
```

## CLI Examples

Monitor a specific process pattern:

```bash
./build/app_top_monitoring nginx
```

Use a longer refresh interval:

```bash
./build/app_top_monitoring --interval 5
```

Show complete command lines:

```bash
./build/app_top_monitoring --long-args
```

Limit the number of displayed processes:

```bash
./build/app_top_monitoring --limit 20
```

Run a single scan:

```bash
./build/app_top_monitoring --once
```

Export CSV:

```bash
./build/app_top_monitoring --csv
```

Export JSON:

```bash
./build/app_top_monitoring --json
```

Capture 30 seconds of metrics and generate a chart report:

```bash
./build/app_top_monitoring --capture=30
```

### Remote Examples

Monitor a card over SSH (key-based login):

```bash
./build/app_top_monitoring --protocol ssh --host 192.168.1.50 --user root
```

Monitor a card over Telnet:

```bash
./build/app_top_monitoring --protocol telnet --host 192.168.1.50 --user root
```

Deploy the binary to a card over FTP:

```bash
./build/app_top_monitoring --protocol ftp --host 192.168.1.50 --user root \
    --local-file ./build/app_top_monitoring
```

Deploy the binary to a card over TFTP (no authentication):

```bash
./build/app_top_monitoring --protocol tftp --host 192.168.1.50 \
    --local-file ./build/app_top_monitoring
```

See [`docs/REMOTE_TESTING.md`](docs/REMOTE_TESTING.md) to try these against local test servers before pointing them at real hardware.

## Design Goals

The project is being developed with the following goals:

1. **Professional architecture**

   Keep process collection, metrics, filtering, output, remote transport, terminal UI, and graphical UI separated.

2. **Reusable backend**

   The Linux monitoring engine should not depend on Qt.

3. **Low overhead**

   Process information is read directly from `/proc` without unnecessary external commands. Remote transport shells out only to standard, already-present client tools rather than linking new libraries.

4. **Interactive monitoring**

   The CLI should remain useful for terminal environments while the Qt application provides a richer desktop experience, including remote cards.

5. **Extensibility**

   New metrics, filters, sorting modes, output formats, remote protocols, and UI features should be possible without redesigning the entire application.

6. **Clear separation of concerns**

```text
Core
 ├── Process collection
 ├── Metrics
 ├── Filtering
 ├── Output
 └── Remote (SSH/Telnet/FTP/TFTP)

CLI
 └── Terminal interaction

Qt
 ├── Controller
 ├── Model
 ├── Main window
 ├── Settings
 ├── Remote dialog
 └── Theme
```

## Version

The `VERSION` file at the project root is the single source of truth — CMake reads it and bakes it into both binaries, so bumping a release is a one-line change to that one file. See [ARCHITECTURE.md](docs/ARCHITECTURE.md#version-management) for how it flows through the build.

Current version:

```text
1.0.0
```

Check it at runtime: `app_top_monitoring --version` (CLI), or the title bar / **Help → About** (Qt).

## Development Status

The project currently contains:

* C Linux monitoring backend
* `/proc` process scanning
* Process metrics
* Process filtering
* Sorting
* CLI output
* Interactive terminal UI
* Remote monitoring over SSH and Telnet
* Remote deployment over FTP and TFTP
* Time-series capture (`--capture`) with an interactive HTML/Chart.js report
* Qt 6 application
* Qt process model
* Qt monitoring controller
* Qt main window
* Qt settings dialog
* Qt remote connectivity dialog
* Qt light/dark theming
* Qt menus and desktop controls
* Staged GitHub Actions CI/CD pipeline
* Single-source-of-truth version management (`VERSION` file, displayed in both CLI and Qt)
* Full documentation set under `docs/`

The Qt interface is being developed incrementally on top of the existing C monitoring engine.

## License

License information can be added here when the project license is selected.
