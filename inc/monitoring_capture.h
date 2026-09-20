#ifndef MONITORING_CAPTURE_H
#define MONITORING_CAPTURE_H

#include "monitoring_services.h"

/*
 * One time-series sample: system-wide CPU/memory/swap utilization plus
 * the aggregate I/O rate and count of whatever processes matched the
 * active filters at that moment.
 */
typedef struct {
  double elapsed_seconds;
  double cpu_percent;
  double mem_percent;
  double swap_percent;
  double io_read_mb_s;
  double io_write_mb_s;
  size_t process_count;
} NeoCaptureSample;

typedef struct {
  NeoCaptureSample *items;
  size_t count;
  size_t capacity;
  time_t start_time;
} NeoCaptureSeries;

void capture_series_init(NeoCaptureSeries *series);
void capture_series_free(NeoCaptureSeries *series);

/*
 * Reads current system-wide CPU/memory/swap utilization directly from
 * /proc/stat and /proc/meminfo (independent of NeoSystemInfo, which
 * does not track idle time or available memory), combines it with the
 * aggregate I/O and count of the given (already filtered/sorted)
 * process list, and appends one sample to `series`.
 *
 * CPU utilization requires a previous sample to compute a delta; the
 * very first call in a capture session records 0.0 for cpu_percent,
 * matching how per-process CPU% is already handled elsewhere.
 *
 * Returns 0 on success, -1 on failure.
 */
int capture_sample(NeoCaptureSeries *series, const NeoProcessList *list);

/*
 * Writes the captured series as a CSV file.
 */
int capture_write_csv(const NeoCaptureSeries *series, const char *path);

/*
 * Writes the captured series as a self-contained HTML report: one
 * line chart per metric (CPU / Memory / Swap / I/O) in a single page,
 * each with a legend, plus a dropdown to show all graphs at once or
 * isolate a single one. Uses Chart.js from a CDN, so viewing the
 * report later requires internet access (the report itself has no
 * other dependency and can be copied anywhere).
 */
int capture_write_html_report(const NeoCaptureSeries *series, const char *path);

/*
 * Reads current system-wide CPU/memory/swap utilization directly,
 * without appending to a series - useful for a live/rolling display
 * that keeps its own bounded history (e.g. the Qt "Live Graphs"
 * window) instead of an ever-growing NeoCaptureSeries.
 *
 * CPU utilization needs a delta between two calls; the very first
 * call in a process's lifetime reports 0.0 for cpu_percent, matching
 * capture_sample()'s behavior (and sharing the same internal state,
 * so mixing calls to both in one process is fine).
 */
void capture_read_system(double *cpu_percent, double *mem_percent,
                         double *swap_percent);

#endif /* MONITORING_CAPTURE_H */
