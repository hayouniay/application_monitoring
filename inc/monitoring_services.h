#ifndef NEO_MONITORING_SERVICES_H
#define NEO_MONITORING_SERVICES_H

#define _GNU_SOURCE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* Version / global limits                                                   */
/* ------------------------------------------------------------------------- */

#define NEO_VERSION "1.0.0"

#define NEO_DEFAULT_INTERVAL 2.0
#define NEO_MIN_INTERVAL 0.1
#define NEO_MAX_INTERVAL 60.0

#define NEO_MAX_PATTERNS 128
#define NEO_MAX_CMDLINE 8192
#define NEO_MAX_USER 64
#define NEO_MAX_STATE_FILTER 16
#define NEO_INITIAL_CAPACITY 128

/* ------------------------------------------------------------------------- */
/* Sorting                                                                   */
/* ------------------------------------------------------------------------- */

typedef enum {
  NEO_SORT_CPU = 0,
  NEO_SORT_MEM,
  NEO_SORT_PID,
  NEO_SORT_RSS,
  NEO_SORT_IO_READ,
  NEO_SORT_IO_WRITE
} NeoSortMode;

/* ------------------------------------------------------------------------- */
/* Process information                                                       */
/* ------------------------------------------------------------------------- */

typedef struct {
  pid_t pid;
  pid_t ppid;
  uid_t uid;

  char user[NEO_MAX_USER];

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
  char cmdline[NEO_MAX_CMDLINE];

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

  /* User filter */
  uid_t filter_uid;
  bool filter_uid_enabled;

  char filter_user[NEO_MAX_USER];
  bool filter_user_enabled;

  /* PID filters */
  pid_t *pids;
  size_t pid_count;

  /* PPID filters */
  pid_t *ppids;
  size_t ppid_count;

  /* Process state filter */
  char states[NEO_MAX_STATE_FILTER];
  size_t state_count;

  /* Sorting */
  NeoSortMode sort_mode;

  /* Include patterns */
  char **patterns;
  size_t pattern_count;

  /* Exclude patterns */
  char **exclude_patterns;
  size_t exclude_count;

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

#endif /* NEO_MONITORING_SERVICES_H */
