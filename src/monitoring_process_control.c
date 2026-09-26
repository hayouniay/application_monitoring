#define _GNU_SOURCE

#include "monitoring_process_control.h"
#include "monitoring_log.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

static void set_message(char *message, size_t message_size, const char *fmt,
                        ...) {
  va_list args;

  if (message == NULL || message_size == 0) {
    return;
  }

  va_start(args, fmt);
  vsnprintf(message, message_size, fmt, args);
  va_end(args);
}

static const char *signal_name(int signal_number) {
  switch (signal_number) {
  case SIGTERM:
    return "SIGTERM";
  case SIGKILL:
    return "SIGKILL";
  case SIGINT:
    return "SIGINT";
  case SIGHUP:
    return "SIGHUP";
  case SIGSTOP:
    return "SIGSTOP";
  case SIGCONT:
    return "SIGCONT";
  default:
    return "signal";
  }
}

int process_send_signal(pid_t pid, int signal_number, char *message,
                        size_t message_size) {
  if (pid <= 0) {
    set_message(message, message_size, "Invalid PID %d", (int)pid);
    log_write(NEO_LOG_ERROR, "Refused to send %s to invalid PID %d",
             signal_name(signal_number), (int)pid);
    return -1;
  }

  if (kill(pid, signal_number) != 0) {
    const int saved_errno = errno;

    set_message(message, message_size, "Failed to send %s to PID %d: %s",
               signal_name(signal_number), (int)pid, strerror(saved_errno));

    log_write(NEO_LOG_ERROR, "Failed to send %s to PID %d: %s",
             signal_name(signal_number), (int)pid, strerror(saved_errno));

    return -1;
  }

  log_write(NEO_LOG_INFO, "Sent %s to PID %d", signal_name(signal_number),
           (int)pid);

  return 0;
}

int process_renice(pid_t pid, int priority, char *message,
                   size_t message_size) {
  if (pid <= 0) {
    set_message(message, message_size, "Invalid PID %d", (int)pid);
    log_write(NEO_LOG_ERROR, "Refused to renice invalid PID %d", (int)pid);
    return -1;
  }

  if (priority < -20 || priority > 19) {
    set_message(message, message_size,
               "Invalid niceness %d (must be -20..19)", priority);
    return -1;
  }

  errno = 0;

  if (setpriority(PRIO_PROCESS, (id_t)pid, priority) != 0 && errno != 0) {
    const int saved_errno = errno;

    set_message(message, message_size, "Failed to renice PID %d to %d: %s",
               (int)pid, priority, strerror(saved_errno));

    log_write(NEO_LOG_ERROR, "Failed to renice PID %d to %d: %s", (int)pid,
             priority, strerror(saved_errno));

    return -1;
  }

  log_write(NEO_LOG_INFO, "Reniced PID %d to %d", (int)pid, priority);

  return 0;
}
