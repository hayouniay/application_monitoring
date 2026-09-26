#ifndef MONITORING_LOG_H
#define MONITORING_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Log levels                                                                 */
/* ------------------------------------------------------------------------- */

typedef enum {
  NEO_LOG_ERROR = 0,
  NEO_LOG_WARN,
  NEO_LOG_INFO,
  NEO_LOG_DEBUG
} NeoLogLevel;

/* Longest formatted message this module will store/deliver (both to the
 * log file and to sinks/ring buffer). Longer messages are truncated. */
#define NEO_LOG_MESSAGE_MAX 512

/* ------------------------------------------------------------------------- */
/* Level helpers (used by CLI argument parsing)                              */
/* ------------------------------------------------------------------------- */

/*
 * Parses "error", "warn"/"warning", "info" or "debug" (case-insensitive)
 * into *out. Returns 0 on success, -1 if `text` is not a known level.
 */
int log_level_parse(const char *text, NeoLogLevel *out);

/* Short lowercase name for a level ("error", "warn", "info", "debug"). */
const char *log_level_name(NeoLogLevel level);

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                  */
/* ------------------------------------------------------------------------- */

/*
 * Configures the module. `path` may be NULL/empty to leave file logging
 * disabled (log_write() will still update the in-memory ring buffer and
 * notify any registered sinks - this is what lets the Qt app show a
 * live Logs pane without the user ever passing --log-file). `level` is
 * the minimum severity that is recorded/delivered at all (NEO_LOG_DEBUG
 * is the most verbose).
 *
 * Safe to call more than once (e.g. to change the level later); reopens
 * the file if the path changed. Returns 0 on success, -1 if `path` was
 * given but could not be opened for appending.
 */
int log_init(const char *path, NeoLogLevel level);

/* Flushes and closes the log file (if any). Sinks/ring buffer are left
 * intact. Safe to call even if log_init() was never called. */
void log_shutdown(void);

/* Returns true once a log file has successfully been opened. */
bool log_file_enabled(void);

/* ------------------------------------------------------------------------- */
/* Writing                                                                    */
/* ------------------------------------------------------------------------- */

/*
 * Formats and timestamps a log entry. If `level` is more severe than or
 * equal to the configured threshold (i.e. level <= configured level in
 * this enum's ordering) the entry is appended to the log file (when
 * enabled), pushed into the bounded in-memory history, and handed to
 * every registered sink. Thread-safe: intended to be called from any
 * thread, including the background threads the Qt app uses for
 * SSH/FTP/TFTP work, without any extra locking by the caller.
 */
void log_write(NeoLogLevel level, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* ------------------------------------------------------------------------- */
/* History (lets a newly-opened Logs pane pre-populate itself)               */
/* ------------------------------------------------------------------------- */

typedef struct {
  time_t when;
  NeoLogLevel level;
  char message[NEO_LOG_MESSAGE_MAX];
} NeoLogEntry;

/*
 * Copies up to `max_count` of the most recent log entries (oldest
 * first) into `out`. Returns the number of entries copied.
 */
size_t log_copy_recent(NeoLogEntry *out, size_t max_count);

/* ------------------------------------------------------------------------- */
/* Sinks (used by the Qt Logs pane; the CLI has no need for these)           */
/* ------------------------------------------------------------------------- */

typedef void (*NeoLogSink)(const NeoLogEntry *entry, void *user_data);

/* Up to this many sinks may be registered at once - plenty for a single
 * GUI process. */
#define NEO_LOG_MAX_SINKS 4

/*
 * Registers a callback invoked (on whichever thread called log_write())
 * for every entry that passes the level filter, in addition to it being
 * recorded normally. Returns 0 on success, -1 if the sink table is
 * full or the arguments are invalid.
 */
int log_add_sink(NeoLogSink sink, void *user_data);

/* Removes a previously registered sink (matched by both function
 * pointer and user_data). No-op if it isn't currently registered. */
void log_remove_sink(NeoLogSink sink, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* MONITORING_LOG_H */
