#define _GNU_SOURCE

#include "monitoring_alert.h"
#include "monitoring_log.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void alert_state_init(NeoAlertState *state) {
  if (state == NULL) {
    return;
  }

  memset(state, 0, sizeof(*state));
}

/* Evaluates a single metric against its threshold/sustain window,
 * updating the breach-tracking fields in place. Returns true if this
 * call is the moment it newly fired. */
static bool evaluate_one(double value, double threshold, double sustain_seconds,
                         bool *breached, time_t *breach_start, bool *fired) {
  const time_t now = time(NULL);

  if (threshold <= 0.0) {
    /* Disabled: make sure state can't be stuck "breached" from an
     * earlier config. */
    *breached = false;
    *fired = false;
    return false;
  }

  if (value < threshold) {
    *breached = false;
    *fired = false;
    return false;
  }

  if (!*breached) {
    *breached = true;
    *breach_start = now;
    *fired = false;
  }

  if (*fired) {
    return false;
  }

  if (difftime(now, *breach_start) >= sustain_seconds) {
    *fired = true;
    return true;
  }

  return false;
}

int alert_evaluate(const NeoConfig *config, NeoAlertState *state,
                   double cpu_percent, double mem_percent) {
  int mask = 0;
  double sustain;

  if (config == NULL || state == NULL) {
    return 0;
  }

  sustain =
      config->alert_sustain_seconds > 0.0 ? config->alert_sustain_seconds : 0.0;

  if (evaluate_one(cpu_percent, config->alert_cpu_percent, sustain,
                   &state->cpu_breached, &state->cpu_breach_start,
                   &state->cpu_fired)) {
    mask |= NEO_ALERT_CPU;
  }

  if (evaluate_one(mem_percent, config->alert_mem_percent, sustain,
                   &state->mem_breached, &state->mem_breach_start,
                   &state->mem_fired)) {
    mask |= NEO_ALERT_MEM;
  }

  return mask;
}

bool alert_state_is_active(const NeoAlertState *state) {
  if (state == NULL) {
    return false;
  }

  return state->cpu_breached || state->mem_breached;
}

/* Runs `argv[0]` with the given arguments (no shell involved) and waits
 * for it to finish, discarding its output. Used for both `notify-send`
 * and `curl`, which are both short-lived, best-effort, fire-and-forget
 * calls here. Returns 0 if the child ran and exited 0, -1 otherwise
 * (including "command not found", which is expected/harmless when
 * notify-send or curl simply isn't installed). */
static int run_fire_and_forget(char *const argv[]) {
  pid_t child;
  int status = 0;

  child = fork();

  if (child < 0) {
    return -1;
  }

  if (child == 0) {
    /* Child: silence stdout/stderr, then exec. */
    int devnull = open("/dev/null", O_WRONLY);

    if (devnull >= 0) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);

      if (devnull > STDERR_FILENO) {
        close(devnull);
      }
    }

    execvp(argv[0], argv);

    _exit(127);
  }

  if (waitpid(child, &status, 0) != child) {
    return -1;
  }

  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static void notify_desktop(const char *title, const char *body) {
  char *argv[] = {(char *)"notify-send", (char *)"-u", (char *)"critical",
                  (char *)title,         (char *)body, NULL};

  if (run_fire_and_forget(argv) != 0) {
    log_write(NEO_LOG_DEBUG,
              "Desktop notification not sent (is notify-send installed?)");
  }
}

static void post_webhook(const char *url, const char *json_payload) {
  char *argv[] = {(char *)"curl", (char *)"-s",
                  (char *)"-m",   (char *)"5",
                  (char *)"-X",   (char *)"POST",
                  (char *)"-H",   (char *)"Content-Type: application/json",
                  (char *)"-d",   (char *)json_payload,
                  (char *)url,    NULL};

  if (run_fire_and_forget(argv) != 0) {
    log_write(NEO_LOG_WARN, "Alert webhook POST to %s failed", url);
  } else {
    log_write(NEO_LOG_INFO, "Alert webhook POST to %s sent", url);
  }
}

void alert_dispatch(const NeoConfig *config, int fired_mask, double cpu_percent,
                    double mem_percent) {
  char title[128];
  char body[256];
  char payload[512];
  char timestamp[32];
  struct tm tm_buf;
  time_t now;

  if (config == NULL || fired_mask == 0) {
    return;
  }

  now = time(NULL);
  localtime_r(&now, &tm_buf);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &tm_buf);

  if (fired_mask & NEO_ALERT_CPU) {
    snprintf(title, sizeof(title), "app_top_monitoring: CPU threshold");
    snprintf(body, sizeof(body),
             "CPU usage %.1f%% has stayed above %.1f%% for %.0fs", cpu_percent,
             config->alert_cpu_percent, config->alert_sustain_seconds);

    log_write(NEO_LOG_WARN, "ALERT: %s", body);

    if (config->alert_notify) {
      notify_desktop(title, body);
    }

    if (config->alert_webhook[0] != '\0') {
      snprintf(payload, sizeof(payload),
               "{\"alert\":\"cpu\",\"value\":%.2f,\"threshold\":%.2f,"
               "\"sustained_seconds\":%.0f,\"timestamp\":\"%s\"}",
               cpu_percent, config->alert_cpu_percent,
               config->alert_sustain_seconds, timestamp);

      post_webhook(config->alert_webhook, payload);
    }
  }

  if (fired_mask & NEO_ALERT_MEM) {
    snprintf(title, sizeof(title), "app_top_monitoring: Memory threshold");
    snprintf(body, sizeof(body),
             "Memory usage %.1f%% has stayed above %.1f%% for %.0fs",
             mem_percent, config->alert_mem_percent,
             config->alert_sustain_seconds);

    log_write(NEO_LOG_WARN, "ALERT: %s", body);

    if (config->alert_notify) {
      notify_desktop(title, body);
    }

    if (config->alert_webhook[0] != '\0') {
      snprintf(payload, sizeof(payload),
               "{\"alert\":\"memory\",\"value\":%.2f,\"threshold\":%.2f,"
               "\"sustained_seconds\":%.0f,\"timestamp\":\"%s\"}",
               mem_percent, config->alert_mem_percent,
               config->alert_sustain_seconds, timestamp);

      post_webhook(config->alert_webhook, payload);
    }
  }
}
