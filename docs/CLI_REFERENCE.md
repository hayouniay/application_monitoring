# CLI Reference

Complete flag reference for `app_top_monitoring`. For task-oriented examples, see the [README](../Readme.md#cli-examples). This document lists every flag exhaustively, grouped by purpose.

A bare positional argument (`app_top_monitoring nginx`) is shorthand for an include pattern — equivalent to matching `nginx` against each process's command name and command line.

## Display and refresh

| Flag | Argument | Description |
| --- | --- | --- |
| `-i`, `--interval` | seconds | Refresh interval (default `2.0`). Also used as the per-sample interval for `--capture`. |
| `-l`, `--long-args` | | Show the full command line instead of just the command name. |
| `--swap` | | Show the swap column. |
| `--io` | | Show I/O read/write rate columns. |
| `--threads` | | Show the thread-count column. |
| `--start-time` | | Show each process's start time. |
| `--elapsed` | | Show each process's elapsed runtime. |
| `-R`, `--reverse` | | Reverse the current sort order. |
| `-n`, `--limit` | count | Only display the first N processes after sorting (`0` = unlimited). |

## Filtering

| Flag | Argument | Description |
| --- | --- | --- |
| `-x`, `--exclude` | pattern | Exclude processes whose command name or command line contains `pattern` (case-insensitive substring). Exclusions take priority over include patterns. |
| `-p`, `--pid` | PID[,PID...] | Only show these PIDs. |
| `-P`, `--ppid` | PPID[,PPID...] | Only show children of these PPIDs. |
| `-u`, `--user` | user or UID | **Local mode**: filter to this user's processes. **Remote mode** (`--protocol ssh\|telnet\|ftp`): the login username instead — not validated as a local account, since it doesn't need to exist locally. |
| `-s`, `--state` | e.g. `R,S,D,T,Z,I` | Only show processes in these states. |
| `--cpu-threshold` | percent | Hide processes below this CPU%. |
| `--ram-mb` | MB | Hide processes below this RSS. |
| `--ram-percent` | percent | Hide processes below this memory%. |

Filters apply to remote SSH/Telnet results too (fetched from the card, then filtered locally) — see [Remote connectivity](#remote-connectivity-sshtelnetftptftp) below.

## Sorting

| Flag | Argument | Description |
| --- | --- | --- |
| `--sort` | `cpu`\|`mem`\|`pid`\|`rss`\|`io_read`\|`io_write` | Sort mode (default `cpu`). |
| `-R`, `--reverse` | | Reverse the sort order. |

## Execution mode

| Flag | Argument | Description |
| --- | --- | --- |
| `-1`, `--once` | | Run a single scan and exit. |
| `-b`, `--batch` | | Non-interactive batch mode (also implied by `--csv`/`--json`). |
| `--csv` | | CSV output instead of the interactive table. |
| `--json` | | JSON output instead of the interactive table. |
| `--no-color` | | Disable terminal colors. |
| `-h`, `--help` | | Show usage and exit. |
| `-V`, `--version` | | Show the version (from the `VERSION` file — see [Version management](ARCHITECTURE.md#version-management)) and exit. |

## Interactive keys

Available in the default interactive table mode (not `--once`/`--batch`/`--csv`/`--json`):

| Key | Action |
| --- | --- |
| `q` | Quit |
| `c` | Sort by CPU |
| `m` | Sort by memory |
| `p` | Sort by PID |
| `+` | Increase refresh interval |
| `-` | Decrease refresh interval |
| `r` | Force refresh |
| `v` | Reverse sort order |

## Remote connectivity (SSH/Telnet/FTP/TFTP)

| Flag | Argument | Description |
| --- | --- | --- |
| `--protocol` | `local`\|`ssh`\|`telnet`\|`ftp`\|`tftp` | Selects the mode (default `local`). |
| `--host` | address | Remote card address. Required for any non-`local` protocol. |
| `-u`, `--user` | username | Login username (ssh/telnet/ftp). |
| `--password` | password | Login password (telnet/ftp only; ssh uses key-based auth exclusively). |
| `--port` | number | Override the protocol's default port (22 ssh, 23 telnet, 21 ftp, 69 tftp). |
| `--identity` | path | SSH private key (optional; ssh only). |
| `--remote-bin` | name/path | Name or path of `app_top_monitoring` on the card (ssh/telnet monitoring; default `app_top_monitoring`, must already be on the card — see `--protocol ftp\|tftp` to deploy it). |
| `--local-file` | path | File to upload (ftp/tftp deploy). |
| `--remote-file` | name | Destination filename on the card (ftp/tftp deploy; defaults to the local filename). |

`--capture` combined with a non-`local` `--protocol` is rejected with an error rather than silently doing the wrong thing. See [REMOTE_TESTING.md](REMOTE_TESTING.md) for setting up local test servers for each protocol.

## Saved remote targets

Retyping the same `--protocol ssh --host 192.168.1.50 --user root --identity ~/.ssh/id_rsa` for every connection gets old fast. A small JSON file holds named connection profiles instead:

| Flag | Argument | Description |
| --- | --- | --- |
| `--target` | name | Load a saved target's connection settings as defaults for this run. |
| `--save-target` | name | Save this run's connection settings (after any `--target` defaults and overrides below) under this name, then continue running normally. |
| `--delete-target` | name | Delete a saved target and exit. |
| `--list-targets` | | List every saved target and exit. |
| `--targets-file` | path | Use this file instead of the default location. Mainly useful for testing. |

By default, targets live in `$XDG_CONFIG_HOME/neo-monitoring/targets.json`, or `~/.config/neo-monitoring/targets.json` if `$XDG_CONFIG_HOME` isn't set. The file is written with `0600` permissions since a saved target may include a plaintext Telnet/FTP password (SSH always uses key-based auth, never a saved password).

`--target` only fills in fields you didn't already give explicitly — so `--target office-pi --port 2222` connects to `office-pi`'s saved host/user/identity but on port 2222, regardless of which flag comes first on the command line. Combine `--target` and `--save-target` in one command to load, tweak, and re-save a target in one step:

```
app_top_monitoring --target office-pi --port 2222 --save-target office-pi
```

Saving requires a non-`local` `--protocol` (there's nothing remote to save otherwise). The Qt app has a native equivalent — see [Qt GUI Guide](QT_GUI_GUIDE.md#connect-to-remote-dialog).

## Capture (time-series + chart report)

| Flag | Argument | Description |
| --- | --- | --- |
| `--capture` | | Capture until Ctrl+C. |
| `--capture=SECONDS` | seconds | Capture for `SECONDS` then stop automatically. **Note the `=`** — required by `getopt` for an optional argument. `--capture 30` (space-separated) does **not** work as a duration; `30` is instead read as an unrelated include pattern. |
| `--capture-output` | base path | Base path for the two output files (no extension). Default: `capture_<unix-timestamp>` in the current directory. |

Writes `PATH.csv` (raw samples: `elapsed_seconds,cpu_percent,mem_percent,swap_percent,io_read_mb_s,io_write_mb_s,process_count`) and `PATH.html` (an interactive Chart.js report — requires internet access to view, since Chart.js loads from a CDN). Currently local monitoring only.

Metrics are system-wide (CPU/memory/swap read directly from `/proc/stat`/`/proc/meminfo`, independent of per-process figures) plus the aggregate I/O rate and count of whatever processes match the active filters at that sample. The very first sample always reports `0.0` for CPU%, since it needs a delta between two readings.

The Qt app has a native equivalent (**View → Capture...**) that doesn't need a browser — see the [Qt GUI Guide](QT_GUI_GUIDE.md#capture).

## Logging

| Flag | Argument | Description |
| --- | --- | --- |
| `--log-file` | path | Append timestamped log entries to this file. Default: disabled (nothing is written; behavior is unchanged from before this flag existed). |
| `--log-level` | `error`\|`warn`\|`info`\|`debug` | Minimum severity recorded (default `info`). |

Everything still goes to stdout/stderr as before — `--log-file` is additive, for post-mortem debugging, and records: refresh errors, remote connection attempts/failures (SSH/Telnet/FTP/TFTP), and capture start/stop. Each line is `[YYYY-MM-DD HH:MM:SS] [LEVEL] message`.

The Qt app has a native equivalent (**View → Logs...**) showing the same live stream in-process, including messages from background SSH/FTP/TFTP threads that would otherwise only appear in the remote dialog's own log box while it happens to be open — see the [Qt GUI Guide](QT_GUI_GUIDE.md).

## Threshold alerting

| Flag | Argument | Description |
| --- | --- | --- |
| `--alert-cpu` | percent | Alert when system-wide CPU% stays above this value. Unset/`0` = disabled. |
| `--alert-mem` | percent | Alert when system-wide memory% stays above this value. Unset/`0` = disabled. |
| `--alert-duration` | seconds | How long a threshold must be continuously exceeded before it fires (default `5`) — avoids alerting on a brief spike. |
| `--alert-notify` | | Send a desktop notification (via `notify-send`, if installed) when an alert fires. |
| `--alert-webhook` | URL | POST a small JSON payload (`{"alert":"cpu","value":...,"threshold":...,"sustained_seconds":...,"timestamp":...}`) to this URL (via `curl`, if installed) when an alert fires. |

In `--once`/`--batch`/`--csv`/`--json` modes (and in the continuous mode's normal Ctrl+C exit), the process exits with code **2** instead of 0 if any alert fired during the run — useful for scripts and process supervisors without having to scrape output. Alert conditions are logged (see [Logging](#logging) above) regardless of whether `--log-file` is set.

The Qt app has a native equivalent (**View → Alert Thresholds...**), dispatching via a system tray notification and/or webhook instead of `notify-send`/exit code.

