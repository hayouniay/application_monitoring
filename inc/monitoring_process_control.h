#ifndef MONITORING_PROCESS_CONTROL_H
#define MONITORING_PROCESS_CONTROL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local process control: sending signals and changing scheduling
 * priority. Deliberately NOT wired up for remote (SSH/Telnet) targets -
 * acting on a process over a remote monitoring connection is a very
 * different trust decision than merely observing it, so that stays out
 * of scope here and is enforced by callers (the Qt UI disables these
 * actions while viewing a remote process list) rather than by this
 * module, which only ever touches the local machine via kill()/
 * setpriority() regardless.
 */

/*
 * Sends POSIX signal `signal_number` (e.g. SIGTERM, SIGKILL) to `pid`.
 * On failure, writes a human-readable reason into `message` (may be
 * NULL to discard it). Returns 0 on success, -1 on failure.
 */
int process_send_signal(pid_t pid, int signal_number, char *message,
                        size_t message_size);

/*
 * Sets `pid`'s scheduling priority ("niceness") to `priority`
 * (-20 = highest priority, 19 = lowest). On failure, writes a
 * human-readable reason into `message` (may be NULL to discard it).
 * Returns 0 on success, -1 on failure.
 */
int process_renice(pid_t pid, int priority, char *message,
                   size_t message_size);

#ifdef __cplusplus
}
#endif

#endif /* MONITORING_PROCESS_CONTROL_H */
