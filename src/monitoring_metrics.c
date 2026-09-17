#define _GNU_SOURCE

#include "monitoring_metrics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static unsigned long long read_total_cpu(void) {
  FILE *fp;
  char line[1024];

  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  fp = fopen("/proc/stat", "r");

  if (fp == NULL) {
    return 0;
  }

  if (fgets(line, sizeof(line), fp) != NULL) {
    (void)sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user,
                 &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
  }

  fclose(fp);

  return user + nice + system + idle + iowait + irq + softirq + steal;
}

/* ------------------------------------------------------------------------- */
/* CPU percentage                                                            */
/* ------------------------------------------------------------------------- */

double metrics_cpu_percent(unsigned long long current_total,
                               unsigned long long previous_total,
                               unsigned long long current_system_total,
                               unsigned long long previous_system_total,
                               unsigned int cpu_count) {
  unsigned long long process_delta;
  unsigned long long system_delta;

  if (current_total < previous_total) {
    return 0.0;
  }

  if (current_system_total < previous_system_total) {
    return 0.0;
  }

  process_delta = current_total - previous_total;

  system_delta = current_system_total - previous_system_total;

  if (system_delta == 0) {
    return 0.0;
  }

  /*
   * Scale by the number of CPUs so that:
   *
   *   100% = one completely utilized CPU
   *
   * This matches the familiar top-style interpretation.
   */
  if (cpu_count == 0) {
    cpu_count = 1;
  }

  return ((double)process_delta / (double)system_delta) * 100.0 *
         (double)cpu_count;
}

/* ------------------------------------------------------------------------- */
/* Memory percentage                                                         */
/* ------------------------------------------------------------------------- */

double metrics_memory_percent(unsigned long long rss_kb,
                                  unsigned long long total_memory_kb) {
  if (total_memory_kb == 0) {
    return 0.0;
  }

  return ((double)rss_kb / (double)total_memory_kb) * 100.0;
}

/* ------------------------------------------------------------------------- */
/* I/O rate                                                                  */
/* ------------------------------------------------------------------------- */

double metrics_io_rate(unsigned long long current_bytes,
                           unsigned long long previous_bytes, double interval) {
  unsigned long long delta;

  if (interval <= 0.0) {
    return 0.0;
  }

  if (current_bytes < previous_bytes) {
    return 0.0;
  }

  delta = current_bytes - previous_bytes;

  return ((double)delta / (1024.0 * 1024.0)) / interval;
}

/* ------------------------------------------------------------------------- */
/* Unit conversion                                                           */
/* ------------------------------------------------------------------------- */

double metrics_kb_to_mb(unsigned long long kb) {
  return (double)kb / 1024.0;
}

/* ------------------------------------------------------------------------- */
/* Process elapsed time                                                      */
/* ------------------------------------------------------------------------- */

double metrics_elapsed_seconds(time_t start_time) {
  time_t now;

  if (start_time <= 0) {
    return 0.0;
  }

  now = time(NULL);

  if (now < start_time) {
    return 0.0;
  }

  return difftime(now, start_time);
}

/* ------------------------------------------------------------------------- */
/* Total process CPU time                                                    */
/* ------------------------------------------------------------------------- */

unsigned long long metrics_total_cpu_time(const NeoProcess *process) {
  if (process == NULL) {
    return 0;
  }

  return process->utime + process->stime;
}

/* ------------------------------------------------------------------------- */
/* Complete metrics update                                                   */
/* ------------------------------------------------------------------------- */

void update_metrics(NeoProcess *process, const NeoPreviousSample *previous,
                        const NeoSystemInfo *system, double interval) {
  unsigned long long current_system_cpu;

  if (process == NULL) {
    return;
  }

  /*
   * Memory.
   */
  if (system != NULL) {
    process->mem_percent =
        metrics_memory_percent(process->rss_kb, system->total_memory_kb);
  } else {
    process->mem_percent = 0.0;
  }

  process->rss_mb = metrics_kb_to_mb(process->rss_kb);

  process->vsz_mb = metrics_kb_to_mb(process->vsz_kb);

  process->swap_mb = metrics_kb_to_mb(process->swap_kb);

  /*
   * I/O.
   */
  if (previous != NULL) {

    process->io_read_mb_s = metrics_io_rate(process->read_bytes,
                                                previous->read_bytes, interval);

    process->io_write_mb_s = metrics_io_rate(
        process->write_bytes, previous->write_bytes, interval);

  } else {

    process->io_read_mb_s = 0.0;
    process->io_write_mb_s = 0.0;
  }

  /*
   * CPU.
   *
   * The public NeoSystemInfo currently contains the current total
   * CPU counter but not the previous system counter. Therefore the
   * first sample has no valid delta available here.
   *
   * The core loop will provide a more precise CPU calculation once
   * the system baseline is established.
   */
  process->cpu_percent = 0.0;

  if (previous != NULL && system != NULL) {

    /*
     * We can at least detect whether the process itself advanced.
     * The complete system-wide delta is maintained by the core loop.
     */
    if (process->total_time >= previous->total_time) {

      unsigned long long process_delta =
          process->total_time - previous->total_time;

      if (interval > 0.0) {

        long ticks = sysconf(_SC_CLK_TCK);

        if (ticks > 0) {

          /*
           * This is a fallback calculation based on elapsed
           * wall-clock time. It is useful when a system CPU
           * baseline is not yet available.
           */
          process->cpu_percent =
              ((double)process_delta / (double)ticks / interval) * 100.0;

          if (system->cpu_count > 1) {

            /*
             * Keep the value within a reasonable top-style
             * range for the process.
             */
            double max_cpu = 100.0 * (double)system->cpu_count;

            if (process->cpu_percent > max_cpu) {
              process->cpu_percent = max_cpu;
            }
          }
        }
      }
    }
  }

  /*
   * Calculate elapsed process lifetime.
   */
  process->elapsed_seconds = metrics_elapsed_seconds(process->start_time);

  /*
   * Silence an otherwise unused internal helper warning when
   * building with aggressive warning flags. The system CPU counter
   * will be consumed by the core implementation as the architecture
   * evolves.
   */
  current_system_cpu = read_total_cpu();
  (void)current_system_cpu;
}
