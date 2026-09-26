#ifndef MONITORING_SERVICES_H
#define MONITORING_SERVICES_H

//#define _GNU_SOURCE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* Version / global limits                                                   */
/* ------------------------------------------------------------------------- */

#include "monitoring_version.h"

/* VERSION is kept as an alias so existing call sites (output headers,
 * Qt's setApplicationVersion(), etc.) don't need to change; the VERSION
 * file at the project root is the actual single source of truth - see
 * inc/monitoring_version.h.in and CMakeLists.txt. */
#define VERSION NEO_VERSION

#define DEFAULT_INTERVAL 2.0
#define MIN_INTERVAL 0.1
#define MAX_INTERVAL 60.0

#define MAX_PATTERNS 128
#define MAX_CMDLINE 8192
#define MAX_USER 64
#define MAX_STATE_FILTER 16
#define INITIAL_CAPACITY 128

/* Remote connectivity limits */
#define MAX_HOST 256
#define MAX_REMOTE_PATH 512

/* Saved remote target name limit (see monitoring_targets.h) - defined
 * here rather than there to avoid a circular include, since NeoConfig
 * below needs it too. */
#define TARGET_NAME_MAX 64

/* ------------------------------------------------------------------------- */
/* Sorting                                                                   */
/* ------------------------------------------------------------------------- */

typedef enum {
  SORT_CPU = 0,
  SORT_MEM,
  SORT_PID,
  SORT_RSS,
  SORT_IO_READ,
  SORT_IO_WRITE
} NeoSortMode;

/* ------------------------------------------------------------------------- */
/* Remote protocol                                                           */
/* ------------------------------------------------------------------------- */

typedef enum {
  PROTOCOL_LOCAL = 0,
  PROTOCOL_SSH,
  PROTOCOL_TELNET,
  PROTOCOL_FTP,
  PROTOCOL_TFTP
} NeoProtocol;

/* ------------------------------------------------------------------------- */
/* Process information                                                       */
/* ------------------------------------------------------------------------- */

typedef struct {
  pid_t pid;
  pid_t ppid;
  uid_t uid;

  char user[MAX_USER];

  char state;
  unsigned long threads;

  /* CPU accounting from /proc/<pid>/stat */
  unsigned long long utime;
  unsigned long long stime;
  unsigned long long total_time;

  /* Disk I/O from /proc/<pid>/io */
  unsigned long long read_bytes;
  unsigned long long write_bytes;

  /* Memory */
  unsigned long long vsz_kb;
  unsigned long long rss_kb;
  unsigned long long swap_kb;

  /* Calculated metrics */
  double cpu_percent;
  double mem_percent;

  double rss_mb;
  double vsz_mb;
  double swap_mb;

  double io_read_mb_s;
  double io_write_mb_s;

  /* Process lifetime */
  time_t start_time;
  double elapsed_seconds;

  /* Names */
  char comm[256];
  char cmdline[MAX_CMDLINE];

} NeoProcess;

/* ------------------------------------------------------------------------- */
/* Process lists                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
  NeoProcess *items;
  size_t count;
  size_t capacity;
} NeoProcessList;

/* ------------------------------------------------------------------------- */
/* Previous samples                                                          */
/* ------------------------------------------------------------------------- */

typedef struct {
  pid_t pid;

  unsigned long long total_time;

  unsigned long long read_bytes;
  unsigned long long write_bytes;

} NeoPreviousSample;

typedef struct {
  NeoPreviousSample *items;
  size_t count;
  size_t capacity;
} NeoPreviousList;

/* ------------------------------------------------------------------------- */
/* Configuration                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {

  /* Refresh */
  double interval;

  /* Thresholds */
  double cpu_threshold;
  double ram_threshold_mb;
  double ram_threshold_percent;

  /* Display */
  bool show_long_args;
  bool show_vsz;
  bool show_swap;
  bool show_io;
  bool show_threads;
  bool show_start_time;
  bool show_elapsed;

  /* Output / execution */
  bool once;
  bool batch;
  bool csv;
  bool json;
  bool no_color;
  bool reverse;

  /* Number of displayed processes */
  size_t limit;

  /* User filter (local mode) */
  uid_t filter_uid;
  bool filter_uid_enabled;

  char filter_user[MAX_USER];
  bool filter_user_enabled;

  /* PID filters */
  pid_t *pids;
  size_t pid_count;

  /* PPID filters */
  pid_t *ppids;
  size_t ppid_count;

  /* Process state filter */
  char states[MAX_STATE_FILTER];
  size_t state_count;

  /* Sorting */
  NeoSortMode sort_mode;

  /* Include patterns */
  char **patterns;
  size_t pattern_count;

  /* Exclude patterns */
  char **exclude_patterns;
  size_t exclude_count;

  /* --------------------------------------------------------------------- */
  /* Remote connectivity (--protocol ssh|telnet|ftp|tftp)                  */
  /* --------------------------------------------------------------------- */

  NeoProtocol protocol;

  /* Login target. `remote_user` doubles as the login username for
   * ssh/telnet/ftp; it does NOT feed the local process user filter
   * above when protocol != PROTOCOL_LOCAL. */
  char remote_host[MAX_HOST];
  char remote_user[MAX_USER];
  char remote_password[MAX_USER];
  int remote_port; /* 0 = use the protocol's default port */

  /* ssh only: optional private key */
  char remote_identity[MAX_REMOTE_PATH];

  /* ssh/telnet: name/path of the monitoring CLI on the remote card */
  char remote_binary[MAX_REMOTE_PATH];

  /* ftp/tftp: file to upload and its destination name */
  char local_file[MAX_REMOTE_PATH];
  char remote_file[MAX_REMOTE_PATH];

  /* --------------------------------------------------------------------- */
  /* Capture (--capture[=SECONDS], --capture-output PATH)                  */
  /* --------------------------------------------------------------------- */

  /* True once --capture is given at all. */
  bool capture_enabled;

  /* 0 = run until SIGINT (Ctrl+C); >0 = stop automatically after this
   * many seconds. */
  double capture_duration_seconds;

  /* Base path (no extension) for the .csv/.html output; empty = an
   * automatic timestamped name in the current directory. */
  char capture_output[MAX_REMOTE_PATH];

  /* --------------------------------------------------------------------- */
  /* Logging (--log-file PATH, --log-level LEVEL)                          */
  /* --------------------------------------------------------------------- */

  /* Empty = file logging disabled (the default: everything still goes
   * to stdout/stderr as before). */
  char log_file[MAX_REMOTE_PATH];

  /* Minimum severity written to the log file / delivered to sinks.
   * Declared as int here (rather than including monitoring_log.h, to
   * keep this header's dependency footprint small) but always holds a
   * valid NeoLogLevel value. */
  int log_level;

  /* --------------------------------------------------------------------- */
  /* Threshold alerting (--alert-cpu, --alert-mem, --alert-duration,       */
  /* --alert-notify, --alert-webhook)                                     */
  /* --------------------------------------------------------------------- */

  /* System-wide CPU%/memory% thresholds; <= 0 disables that check. */
  double alert_cpu_percent;
  double alert_mem_percent;

  /* How long (seconds) a threshold must be continuously exceeded
   * before an alert fires - avoids firing on a brief spike. */
  double alert_sustain_seconds;

  /* Fire a desktop notification (via `notify-send`) when an alert
   * fires. */
  bool alert_notify;

  /* HTTP(S) URL to POST a small JSON payload to when an alert fires;
   * empty = disabled. */
  char alert_webhook[MAX_REMOTE_PATH];

  /* --------------------------------------------------------------------- */
  /* Saved remote targets (--target, --save-target, --list-targets,        */
  /* --delete-target)                                                      */
  /* --------------------------------------------------------------------- */

  /* Name of a saved target to load remote-connection defaults from;
   * empty = none. Applied before the remote-connection fields above so
   * any explicit --host/--user/etc. flag on the same command line
   * still wins - see target_apply_to_config() in monitoring_targets.h. */
  char target_name[TARGET_NAME_MAX];

  /* Non-empty = after parsing, save the (possibly --target-seeded,
   * possibly further overridden) remote-connection fields above as a
   * saved target under this name, then continue normally. */
  char save_target_name[TARGET_NAME_MAX];

  /* Non-empty = delete the named saved target, print a confirmation,
   * and exit (before anything else runs). */
  char delete_target_name[TARGET_NAME_MAX];

  /* True = print every saved target and exit (before anything else
   * runs). */
  bool list_targets;

  /* Overrides the default ~/.config/neo-monitoring/targets.json path;
   * empty = use the default. Mainly useful for testing. */
  char targets_file[MAX_REMOTE_PATH];

} NeoConfig;

/* ------------------------------------------------------------------------- */
/* System information                                                        */
/* ------------------------------------------------------------------------- */

typedef struct {

  /* Total CPU jiffies */
  unsigned long long total_cpu;

  /* Total physical memory in KB */
  unsigned long long total_memory_kb;

  /* Number of online CPUs */
  unsigned int cpu_count;

} NeoSystemInfo;

/* ------------------------------------------------------------------------- */
/* Configuration API                                                         */
/* ------------------------------------------------------------------------- */

void config_init(NeoConfig *config);

void config_free(NeoConfig *config);

int config_parse(NeoConfig *config, int argc, char **argv);

void print_usage(const char *program);

/* ------------------------------------------------------------------------- */
/* Process list API                                                          */
/* ------------------------------------------------------------------------- */

void process_list_init(NeoProcessList *list);

void process_list_free(NeoProcessList *list);

/* ------------------------------------------------------------------------- */
/* Previous sample API                                                       */
/* ------------------------------------------------------------------------- */

void previous_list_init(NeoPreviousList *list);

void previous_list_free(NeoPreviousList *list);

/* ------------------------------------------------------------------------- */
/* System information                                                        */
/* ------------------------------------------------------------------------- */

int read_system_info(NeoSystemInfo *info);

/* ------------------------------------------------------------------------- */
/* Process scanning                                                          */
/* ------------------------------------------------------------------------- */

int scan_processes(const NeoConfig *config, const NeoSystemInfo *system,
                   NeoProcessList *list, NeoPreviousList *previous,
                   double interval);

/* ------------------------------------------------------------------------- */
/* Filtering                                                                 */
/* ------------------------------------------------------------------------- */

int process_matches(const NeoConfig *config, const NeoProcess *process);

/* ------------------------------------------------------------------------- */
/* Metrics                                                                   */
/* ------------------------------------------------------------------------- */

void update_metrics(NeoProcess *process, const NeoPreviousSample *previous,
                    const NeoSystemInfo *system, double interval);

/* ------------------------------------------------------------------------- */
/* Sorting                                                                   */
/* ------------------------------------------------------------------------- */

void sort_processes(NeoProcessList *list, NeoSortMode mode, bool reverse);

/* ------------------------------------------------------------------------- */
/* Terminal UI                                                               */
/* ------------------------------------------------------------------------- */

int ui_setup(void);

void ui_restore(void);

void ui_hide_cursor(void);

void ui_show_cursor(void);

void ui_clear(void);

int ui_read_key(void);

// void ui_wait(double seconds);

int ui_is_interactive(void);

/* ------------------------------------------------------------------------- */
/* Output                                                                    */
/* ------------------------------------------------------------------------- */

void output_table(const NeoConfig *config, const NeoSystemInfo *system,
                  const NeoProcessList *list);

void output_csv(const NeoConfig *config, const NeoProcessList *list);

void output_json(const NeoConfig *config, const NeoSystemInfo *system,
                 const NeoProcessList *list);

#endif /* MONITORING_SERVICES_H */
