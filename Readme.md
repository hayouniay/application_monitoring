# NEO Monitoring Services

A modular Linux process-monitoring application written in C, inspired by `top`.

`monitoring_services` reads process information directly from `/proc` and provides interactive monitoring, filtering, sorting, thresholds, and machine-readable output formats.

---

## Features

- Monitor Linux processes through `/proc`
- Monitor all processes when no pattern is provided
- Case-insensitive process filtering
- Match against:
  - process name (`comm`)
  - full command line
- Display full command-line arguments
- CPU usage monitoring
- Memory usage monitoring
- VSZ monitoring
- RSS monitoring
- Swap monitoring
- I/O read/write monitoring
- Thread count
- Process start time
- Process elapsed time
- PID filtering
- PPID filtering
- UID/user filtering
- Process-state filtering
- Include patterns
- Exclude patterns
- CPU threshold
- RAM threshold in MB
- RAM threshold in percent
- Sorting by:
  - CPU
  - memory
  - PID
  - RSS
  - I/O read
  - I/O write
- Reverse sorting
- Limit displayed processes
- Interactive terminal controls
- One-shot output
- Batch mode
- CSV output
- JSON output
- Optional color output
- Configurable refresh interval

---

## Project Structure

```text
monitoring_services/
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
│   └── monitoring_output.h
│
└── src/
    ├── monotoring_services.c
    ├── monitoring_services_core.c
    ├── monitoring_process.c
    ├── monitoring_metrics.c
    ├── monitoring_filter.c
    ├── monitoring_ui.c
    └── monitoring_output.c

