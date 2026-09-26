#define _GNU_SOURCE

#include "monitoring_log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Ring buffer of recent entries, for the Qt Logs pane's initial history     */
/* ------------------------------------------------------------------------- */

#define LOG_HISTORY_CAPACITY 500

typedef struct {
  NeoLogSink sink;
  void *user_data;
} NeoLogSinkSlot;

static struct {
  pthread_mutex_t lock;

  FILE *file;
  char path[512];

  NeoLogLevel level;

  NeoLogEntry history[LOG_HISTORY_CAPACITY];
  size_t history_count; /* number of valid entries (<= capacity) */
  size_t history_next;  /* next write position (wraps)          */

  NeoLogSinkSlot sinks[NEO_LOG_MAX_SINKS];

  bool initialized;
} g_log = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .file = NULL,
    .path = {0},
    .level = NEO_LOG_INFO,
    .history_count = 0,
    .history_next = 0,
    .sinks = {{0}},
    .initialized = false,
};

/* ------------------------------------------------------------------------- */
/* Level helpers                                                             */
/* ------------------------------------------------------------------------- */

int log_level_parse(const char *text, NeoLogLevel *out) {
  if (text == NULL || out == NULL) {
    return -1;
  }

  if (strcasecmp(text, "error") == 0) {
    *out = NEO_LOG_ERROR;
  } else if (strcasecmp(text, "warn") == 0 ||
             strcasecmp(text, "warning") == 0) {
    *out = NEO_LOG_WARN;
  } else if (strcasecmp(text, "info") == 0) {
    *out = NEO_LOG_INFO;
  } else if (strcasecmp(text, "debug") == 0) {
    *out = NEO_LOG_DEBUG;
  } else {
    return -1;
  }

  return 0;
}

const char *log_level_name(NeoLogLevel level) {
  switch (level) {
  case NEO_LOG_ERROR:
    return "error";
  case NEO_LOG_WARN:
    return "warn";
  case NEO_LOG_INFO:
    return "info";
  case NEO_LOG_DEBUG:
    return "debug";
  default:
    return "?";
  }
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* ------------------------------------------------------------------------- */

int log_init(const char *path, NeoLogLevel level) {
  int result = 0;

  pthread_mutex_lock(&g_log.lock);

  g_log.level = level;

  if (g_log.file != NULL) {
    fclose(g_log.file);
    g_log.file = NULL;
  }

  if (path != NULL && path[0] != '\0') {

    g_log.file = fopen(path, "a");

    if (g_log.file == NULL) {
      g_log.path[0] = '\0';
      result = -1;
    } else {
      /* Line-buffered so entries survive a crash instead of sitting in
       * a stdio buffer - this file exists specifically for post-mortem
       * debugging. */
      setvbuf(g_log.file, NULL, _IOLBF, 0);

      snprintf(g_log.path, sizeof(g_log.path), "%s", path);
    }
  }

  g_log.initialized = true;

  pthread_mutex_unlock(&g_log.lock);

  return result;
}

void log_shutdown(void) {
  pthread_mutex_lock(&g_log.lock);

  if (g_log.file != NULL) {
    fclose(g_log.file);
    g_log.file = NULL;
  }

  g_log.path[0] = '\0';

  pthread_mutex_unlock(&g_log.lock);
}

bool log_file_enabled(void) {
  bool enabled;

  pthread_mutex_lock(&g_log.lock);
  enabled = g_log.file != NULL;
  pthread_mutex_unlock(&g_log.lock);

  return enabled;
}

/* ------------------------------------------------------------------------- */
/* Writing                                                                    */
/* ------------------------------------------------------------------------- */

static void format_timestamp(time_t when, char *out, size_t out_size) {
  struct tm tm_buf;

  localtime_r(&when, &tm_buf);

  strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

void log_write(NeoLogLevel level, const char *fmt, ...) {
  NeoLogEntry entry;
  va_list args;
  NeoLogSinkSlot sinks_copy[NEO_LOG_MAX_SINKS];
  size_t sink_count = 0;
  size_t i;

  if (fmt == NULL) {
    return;
  }

  pthread_mutex_lock(&g_log.lock);

  /* Below the configured threshold: drop it. Higher-severity levels
   * have a *lower* numeric value in NeoLogLevel, so "at least as
   * severe as the threshold" is `level <= g_log.level`. */
  if (level > g_log.level) {
    pthread_mutex_unlock(&g_log.lock);
    return;
  }

  entry.when = time(NULL);
  entry.level = level;

  va_start(args, fmt);
  vsnprintf(entry.message, sizeof(entry.message), fmt, args);
  va_end(args);

  if (g_log.file != NULL) {
    char timestamp[32];

    format_timestamp(entry.when, timestamp, sizeof(timestamp));

    fprintf(g_log.file, "[%s] [%-5s] %s\n", timestamp,
            log_level_name(level), entry.message);
  }

  g_log.history[g_log.history_next] = entry;

  g_log.history_next = (g_log.history_next + 1) % LOG_HISTORY_CAPACITY;

  if (g_log.history_count < LOG_HISTORY_CAPACITY) {
    ++g_log.history_count;
  }

  for (i = 0; i < NEO_LOG_MAX_SINKS; ++i) {
    if (g_log.sinks[i].sink != NULL) {
      sinks_copy[sink_count++] = g_log.sinks[i];
    }
  }

  pthread_mutex_unlock(&g_log.lock);

  /* Sinks are invoked outside the lock so a sink that itself calls back
   * into log_write() (or takes a while, e.g. marshaling onto a Qt event
   * loop) can never deadlock against it. */
  for (i = 0; i < sink_count; ++i) {
    sinks_copy[i].sink(&entry, sinks_copy[i].user_data);
  }
}

/* ------------------------------------------------------------------------- */
/* History                                                                    */
/* ------------------------------------------------------------------------- */

size_t log_copy_recent(NeoLogEntry *out, size_t max_count) {
  size_t count;
  size_t start;
  size_t i;

  if (out == NULL || max_count == 0) {
    return 0;
  }

  pthread_mutex_lock(&g_log.lock);

  count = g_log.history_count < max_count ? g_log.history_count : max_count;

  /* Oldest-of-what-we-return is `count` entries back from the next
   * write position. */
  start = (g_log.history_next + LOG_HISTORY_CAPACITY - count) %
          LOG_HISTORY_CAPACITY;

  for (i = 0; i < count; ++i) {
    out[i] = g_log.history[(start + i) % LOG_HISTORY_CAPACITY];
  }

  pthread_mutex_unlock(&g_log.lock);

  return count;
}

/* ------------------------------------------------------------------------- */
/* Sinks                                                                      */
/* ------------------------------------------------------------------------- */

int log_add_sink(NeoLogSink sink, void *user_data) {
  size_t i;
  int result = -1;

  if (sink == NULL) {
    return -1;
  }

  pthread_mutex_lock(&g_log.lock);

  for (i = 0; i < NEO_LOG_MAX_SINKS; ++i) {
    if (g_log.sinks[i].sink == NULL) {
      g_log.sinks[i].sink = sink;
      g_log.sinks[i].user_data = user_data;
      result = 0;
      break;
    }
  }

  pthread_mutex_unlock(&g_log.lock);

  return result;
}

void log_remove_sink(NeoLogSink sink, void *user_data) {
  size_t i;

  pthread_mutex_lock(&g_log.lock);

  for (i = 0; i < NEO_LOG_MAX_SINKS; ++i) {
    if (g_log.sinks[i].sink == sink && g_log.sinks[i].user_data == user_data) {
      g_log.sinks[i].sink = NULL;
      g_log.sinks[i].user_data = NULL;
      break;
    }
  }

  pthread_mutex_unlock(&g_log.lock);
}
