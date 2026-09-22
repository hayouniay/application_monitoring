# Architecture

This document describes how NEO Monitoring Services is put together internally. For installation and everyday usage, see the [README](../Readme.md).

## Overview

The project is a shared C backend (`monitoring_core`) linked by two independent frontends: a terminal CLI (`app_top_monitoring`) and a Qt 6 desktop app (`app_top_monitoring_qt`). Neither frontend duplicates monitoring logic — both call into the same functions to read `/proc`, compute metrics, filter, sort, and talk to remote cards.

```text
                    ┌─────────────────────────┐
                    │   NEO Monitoring Core   │
                    │       (monitoring_core)  │
                    │                         │
                    │   Linux /proc reading   │
                    │   Process scanning      │
                    │   Metrics computation   │
                    │   Filtering             │
                    │   Sorting               │
                    │   Output (table/CSV/    │
                    │     JSON)               │
                    │   Remote (SSH/Telnet/   │
                    │     FTP/TFTP)           │
                    │   Capture (time series) │
                    └────────────┬────────────┘
                                 │  static library, linked by both
                    ┌────────────┴────────────┐
                    ▼                         ▼
             CLI Application          Qt Desktop Application
        (monotoring_services.c)      (qt/qt_main_window.cpp, etc.)
```

## Module map

| File | Responsibility |
| --- | --- |
| `monitoring_services.h` | Shared types (`NeoConfig`, `NeoProcess`, `NeoProcessList`, `NeoSystemInfo`, `NeoProtocol`) and every public function declaration. This is the contract between the core and both frontends. |
| `monitoring_services_core.c` | `NeoConfig` lifecycle (`config_init`/`config_free`), command-line parsing (`config_parse`), process-list scanning orchestration (`scan_processes`), sorting (`sort_processes`), system info (`read_system_info`). |
| `monitoring_process.c` | Low-level `/proc/<pid>/*` readers: `stat`, `status`, `cmdline`, `io`, start time. |
| `monitoring_metrics.c` | Per-process CPU%/memory%/I/O-rate computation from raw counters and deltas. |
| `monitoring_filter.c` | `process_matches()` and the individual filter predicates (pattern, PID, PPID, user, state). |
| `monitoring_output.c` | Table/CSV/JSON rendering for the CLI. |
| `monitoring_ui.c` | Terminal raw-mode handling and the interactive single-key controls (`q`, `c`, `m`, `p`, `+`, `-`, `r`, `v`). |
| `monitoring_remote.c` | SSH/Telnet monitoring and FTP/TFTP deploy. Shells out to standard client tools (`ssh`, `telnet`, `curl`, `tftp`) via a fork/exec/pipe harness — no networking libraries are linked in. |
| `monitoring_capture.c` | System-wide CPU/memory/swap sampling (independent `/proc/stat`/`/proc/meminfo` readers, since `NeoSystemInfo` doesn't track idle time or available memory), time-series storage, and CSV/HTML report generation. |
| `monitoring_version.h.in` | Template processed by CMake into `monitoring_version.h`; see [Version management](#version-management) below. |

### Qt layer (`inc/qt/`, `src/qt/`)

| File | Responsibility |
| --- | --- |
| `qt_application.*` | `QApplication` subclass: app metadata, style, and the light/dark theme (loads/saves via `QSettings`, applies a QSS stylesheet). |
| `qt_main_window.*` | Top-level window: toolbar, menu bar, process table, filters, stats, and all the wiring between the monitor controller, model, and the various dialogs/windows below. |
| `qt_monitor_controller.*` | Owns the `NeoConfig`, drives a `QTimer`-based refresh loop calling into the core (`read_system_info` → `scan_processes` → `sort_processes`), and exposes results via Qt signals. |
| `qt_process_model.*` | `QAbstractTableModel` wrapping a `QVector<NeoProcess>` for the `QTableView`. Has two `setProcesses()` overloads: one from a `NeoProcessList` (local monitoring) and one from a `QVector<NeoProcess>` (remote results, built on a background thread). |
| `qt_settings_dialog.*` | Exposes the same filter/sort/display options as the CLI flags, editing the live `NeoConfig`. |
| `qt_remote_dialog.*` | The "Connect to Remote" dialog: SSH/Telnet monitoring and FTP/TFTP deploy, all four protocols. Network calls run on a detached `std::thread`; results are marshaled back to the UI thread via `QMetaObject::invokeMethod`. |
| `qt_wave_widget.*` | A dependency-free `QPainter`-drawn line graph (no QtCharts). Supports one or two series, fixed or auto-ranged axes, and an optional real-clock-time X axis. |
| `qt_graphs_window.*` | Always-on "Live Graphs" window: four `NeoQtWaveWidget`s (CPU/Memory/Swap/I-O) fed one sample per local refresh tick, capped at a rolling 120 samples. |
| `qt_capture_window.*` | Bounded "Capture" window: the Qt equivalent of `--capture`. No rolling cap, real-clock-time X axis, and exports the same CSV/HTML pair the CLI produces via the shared `monitoring_capture` functions. |

## Data flow: local monitoring

Both frontends follow the same core loop; only the driver differs (a `while` loop with `sleep()` for the CLI, a `QTimer` for Qt):

```text
read_system_info()  →  scan_processes()  →  sort_processes()  →  output
```

`scan_processes()` internally walks `/proc`, calls `process_collect()` per PID (which in turn calls the `monitoring_process.c` readers), applies `process_matches()` from `monitoring_filter.c`, and computes metrics via `monitoring_metrics.c` using the previous sample's counters for deltas (CPU%, I/O rate). The `NeoPreviousList` carries just enough state (PID, cumulative CPU ticks, cumulative I/O bytes) between refreshes to compute those deltas without keeping full history.

## Data flow: remote monitoring (SSH/Telnet)

Remote monitoring reuses the CLI's own CSV output format as the wire format — the remote card runs its own `app_top_monitoring --csv --once`, and the local side parses the result:

```text
ssh/telnet client (subprocess)
   │  runs: echo <marker>; grep MemTotal /proc/meminfo; nproc;
   │        echo <marker>; app_top_monitoring --csv --once
   ▼
captured stdout (one round trip)
   │
   ├── system info section  →  parse_remote_system_info()  →  NeoSystemInfo
   └── CSV section          →  parse_csv_output()           →  NeoProcessList
```

Bundling the system-info query into the *same* remote command avoids a second connection per refresh. The markers are matched by their *last* occurrence in the captured text rather than the first, because Telnet echoes back the command you typed (which itself contains the marker text) before the real output appears — matching the first occurrence would pick up the echoed command instead of the actual result.

The CSV parser is deliberately lenient: any line that doesn't parse as a well-formed 17-field row (banners, shell prompts, echoed input, even the CSV header itself) is silently skipped rather than treated as an error. This is what makes the same parser work for both a clean SSH session and a noisy Telnet one.

## Threading model (Qt only)

The C backend is entirely synchronous and single-threaded by design — every function blocks until it has an answer. The Qt app keeps its UI responsive around that by pushing blocking calls onto background threads:

- `qt_remote_dialog.cpp` spawns a detached `std::thread` for every SSH/Telnet/FTP/TFTP action. The thread calls the blocking C function, then hands the result back to the UI thread via `QMetaObject::invokeMethod(qApp, lambda, Qt::QueuedConnection)`.
- A `QPointer` guard is captured into each background lambda so that if the dialog is closed (it's `Qt::WA_DeleteOnClose`) while a call is still in flight, the callback safely no-ops instead of touching a destroyed object.
- Local monitoring, Live Graphs, and Capture do **not** need threading — a local `/proc` scan is fast enough to run directly on the `QTimer` tick without blocking the UI noticeably.

## Version management

The `VERSION` file at the project root is the single source of truth. CMake reads it before the `project()` call and passes it as that command's `VERSION` argument, which populates `PROJECT_VERSION` and its `_MAJOR`/`_MINOR`/`_PATCH` components. `configure_file()` then bakes those into `inc/monitoring_version.h.in` → `<build>/generated/monitoring_version.h`, which `monitoring_services.h` includes and aliases as the `VERSION` macro (kept for backward compatibility with existing call sites in `monitoring_output.c` and `qt_application.cpp`). Bumping a release is a one-line change to the `VERSION` file; nothing else needs editing.

## Build graph

```text
VERSION file
   │
   ▼
CMakeLists.txt (project(VERSION ...), configure_file())
   │
   ▼
monitoring_core (static library)
   │
   ├──► app_top_monitoring     (CLI; never touches Qt; BUILD_QT_APP can be OFF)
   │
   └──► app_top_monitoring_qt  (Qt6::Widgets + Threads::Threads)
```

See the [README](../Readme.md#continuous-integration) for how this maps onto the staged GitHub Actions pipeline.
