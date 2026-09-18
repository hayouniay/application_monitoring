#ifndef MONITORING_OUTPUT_H
#define MONITORING_OUTPUT_H

#include "monitoring_services.h"

/*
 * Print the normal interactive table.
 */
void output_table(const NeoConfig *config, const NeoSystemInfo *system,
                  const NeoProcessList *list);

/*
 * Print CSV output.
 */
void output_csv(const NeoConfig *config, const NeoProcessList *list);

/*
 * Print JSON output.
 */
void output_json(const NeoConfig *config, const NeoSystemInfo *system,
                 const NeoProcessList *list);

/*
 * Print a single process as a table row.
 */
void output_process_row(const NeoConfig *config, const NeoProcess *process);

/*
 * Print CSV header.
 */
void output_csv_header(void);

/*
 * Print JSON escaping for arbitrary strings.
 */
void output_json_string(const char *text);

#endif /* MONITORING_OUTPUT_H */
