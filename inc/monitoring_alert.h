#ifndef MONITORING_ALERT_H
#define MONITORING_ALERT_H

#include <stdbool.h>
#include <time.h>

#include "monitoring_services.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bits returned by alert_evaluate() / passed to alert_dispatch(). */
#define NEO_ALERT_CPU 0x1
#define NEO_ALERT_MEM 0x2

/*
 * Exit code used by the CLI (in --once/batch-style runs) when a
 * threshold alert was active/fired during the run, so scripts and
 * process supervisors can tell "ran fine" apart from "ran fine, but
 * something was over threshold" without scraping output.
 */
#define NEO_EXIT_ALERT 2

/*
 * Tracks how long CPU/memory have continuously been over their
 * configured thresholds, so a brief spike doesn't fire an alert - only
 * a breach sustained for config->alert_sustain_seconds does. One of
 * these per monitoring session (CLI process, or Qt monitor
 * controller).
 */
typedef struct {
  bool cpu_breached;
  time_t cpu_breach_start;
  bool cpu_fired;

  bool mem_breached;
  time_t mem_breach_start;
  bool mem_fired;
} NeoAlertState;

void alert_state_init(NeoAlertState *state);

/*
 * Evaluates the current system-wide CPU%/memory% against
 * config->alert_cpu_percent / config->alert_mem_percent (either being
 * <= 0 disables that check). Returns a bitmask (NEO_ALERT_CPU |
 * NEO_ALERT_MEM) of the checks that just transitioned into "fired"
 * this call - i.e. crossed the threshold and stayed over it for at
 * least config->alert_sustain_seconds since the last time it was seen
 * back under threshold. Returns 0 when nothing newly fired (including
 * while a breach is still ongoing but already reported, or hasn't been
 * sustained long enough yet).
 */
int alert_evaluate(const NeoConfig *config, NeoAlertState *state,
                   double cpu_percent, double mem_percent);

/*
 * True if the given mask indicates *any* threshold is currently over
 * its limit right now (regardless of whether it has fired yet) - used
 * by the CLI to decide its exit code at the end of a run.
 */
bool alert_state_is_active(const NeoAlertState *state);

/*
 * Best-effort dispatch of the configured actions (desktop notification
 * via `notify-send`, and/or an HTTP webhook via `curl`) for the checks
 * set in `fired_mask`. Intended for the CLI, where a brief blocking
 * exec is acceptable; the Qt app instead calls alert_evaluate() itself
 * and dispatches natively (QSystemTrayIcon + QProcess) so the UI
 * thread is never blocked - see qt_monitor_controller. Failures here
 * are logged (monitoring_log) but never treated as fatal.
 */
void alert_dispatch(const NeoConfig *config, int fired_mask, double cpu_percent,
                    double mem_percent);

#ifdef __cplusplus
}
#endif

#endif /* MONITORING_ALERT_H */
