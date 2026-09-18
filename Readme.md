# NEO Monitoring Services

A professional Linux process-monitoring application inspired by `top`.

NEO Monitoring Services provides:

* A lightweight terminal-based process monitor.
* A Qt 6 desktop application.
* Linux `/proc` process inspection.
* CPU, memory, RSS, VSZ, swap, and I/O monitoring.
* Process filtering and sorting.
* Configurable refresh intervals.
* Interactive monitoring controls.
* CSV and JSON output modes.
* A reusable C monitoring backend shared by the CLI and Qt application.

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

Pattern matching is case-insensitive substring matching against the process command name and command line.

### Sorting

Available sorting modes include:

* CPU
* Memory
* PID
* RSS
* I/O read
* I/O write

Sorting can also be reversed.

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

The Qt interface uses the same C monitoring backend as the CLI.

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

and linked by both executables.

## Qt Monitoring Flow

The desktop application follows this update flow:

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

This keeps Linux process collection and metric calculation inside the C backend while the Qt layer handles presentation and user interaction.

## Project Structure

```text
app_top_monitoring/
├── CMakeLists.txt
├── Makefile
├── README.md
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
│   │
│   └── qt/
│       ├── qt_application.h
│       ├── qt_main_window.h
│       ├── qt_process_model.h
│       ├── qt_monitor_controller.h
│       └── qt_settings_dialog.h
│
├── src/
│   ├── monotoring_services.c
│   ├── monitoring_services_core.c
│   ├── monitoring_process.c
│   ├── monitoring_metrics.c
│   ├── monitoring_filter.c
│   ├── monitoring_ui.c
│   ├── monitoring_output.c
│   │
│   └── qt/
│       ├── qt_application.cpp
│       ├── qt_main_window.cpp
│       ├── qt_process_model.cpp
│       ├── qt_monitor_controller.cpp
│       └── qt_settings_dialog.cpp
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

### Qt

The desktop application requires:

* Qt 6
* Qt Widgets
* A C++17-compatible compiler

The Qt application is built without Qt Designer. The interface is currently constructed programmatically using Qt Widgets.

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

## Design Goals

The project is being developed with the following goals:

1. **Professional architecture**

   Keep process collection, metrics, filtering, output, terminal UI, and graphical UI separated.

2. **Reusable backend**

   The Linux monitoring engine should not depend on Qt.

3. **Low overhead**

   Process information is read directly from `/proc` without unnecessary external commands.

4. **Interactive monitoring**

   The CLI should remain useful for terminal environments while the Qt application provides a richer desktop experience.

5. **Extensibility**

   New metrics, filters, sorting modes, output formats, and UI features should be possible without redesigning the entire application.

6. **Clear separation of concerns**

```text
Core
 ├── Process collection
 ├── Metrics
 ├── Filtering
 └── Output

CLI
 └── Terminal interaction

Qt
 ├── Controller
 ├── Model
 ├── Main window
 └── Settings
```

## Version

Current project version:

```text
1.0.0
```

## Development Status

The project currently contains:

* C Linux monitoring backend
* `/proc` process scanning
* Process metrics
* Process filtering
* Sorting
* CLI output
* Interactive terminal UI
* Qt 6 application
* Qt process model
* Qt monitoring controller
* Qt main window
* Qt settings dialog
* Qt menus and desktop controls

The Qt interface is being developed incrementally on top of the existing C monitoring engine.

## License

License information can be added here when the project license is selected.
