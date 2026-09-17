#ifndef NEO_MONITORING_METRICS_H
#define NEO_MONITORING_METRICS_H

#include "monitoring_services.h"

/*
 * Calculate CPU, memory and I/O metrics for one process.
 *
 * The previous sample is used to calculate deltas between refreshes.
 */
void update_metrics(NeoProcess *process, const NeoPreviousSample *previous,
                        const NeoSystemInfo *system, double interval);

/*
 * Calculate process CPU percentage from CPU-time deltas.
 */
double metrics_cpu_percent(unsigned long long current_total,
                               unsigned long long previous_total,
                               unsigned long long current_system_total,
                               unsigned long long previous_system_total,
                               unsigned int cpu_count);

/*
 * Calculate memory usage as a percentage of physical RAM.
 */
double metrics_memory_percent(unsigned long long rss_kb,
                                  unsigned long long total_memory_kb);

/*
 * Calculate an I/O rate in MB/s.
 */
double metrics_io_rate(unsigned long long current_bytes,
                           unsigned long long previous_bytes, double interval);

/*
 * Convert KB to MB.
 */
double metrics_kb_to_mb(unsigned long long kb);

/*
 * Calculate elapsed process time.
 */
double metrics_elapsed_seconds(time_t start_time);

/*
 * Return the total CPU time used by a process.
 */
unsigned long long metrics_total_cpu_time(const NeoProcess *process);

#endif /* NEO_MONITORING_METRICS_H */
