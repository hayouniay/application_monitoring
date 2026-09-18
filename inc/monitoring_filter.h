#ifndef MONITORING_FILTER_H
#define MONITORING_FILTER_H

#include "monitoring_services.h"

/*
 * Check whether a process matches all configured filters.
 *
 * Include patterns are OR-based:
 *   pattern A OR pattern B OR ...
 *
 * Exclude patterns always have priority:
 *   if an exclude pattern matches, the process is rejected.
 *
 * Additional filters such as PID, PPID, user and state are
 * applied independently.
 *
 * Returns:
 *   1  process matches
 *   0  process does not match
 */
int process_matches(const NeoConfig *config, const NeoProcess *process);

/*
 * Case-insensitive substring matching.
 *
 * Returns:
 *   1  match
 *   0  no match
 */
int filter_contains_ci(const char *text, const char *pattern);

/*
 * Check whether a process matches one of the configured
 * include patterns using its command name or command line.
 */
int filter_match_patterns(const NeoConfig *config, const NeoProcess *process);

/*
 * Check whether a process matches one of the configured
 * exclusion patterns.
 */
int filter_match_excludes(const NeoConfig *config, const NeoProcess *process);

/*
 * Check PID filters.
 */
int filter_match_pid(const NeoConfig *config, const NeoProcess *process);

/*
 * Check PPID filters.
 */
int filter_match_ppid(const NeoConfig *config, const NeoProcess *process);

/*
 * Check user / UID filters.
 */
int filter_match_user(const NeoConfig *config, const NeoProcess *process);

/*
 * Check process state filters.
 */
int filter_match_state(const NeoConfig *config, const NeoProcess *process);

#endif /* MONITORING_FILTER_H */
