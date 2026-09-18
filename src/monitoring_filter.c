#define _GNU_SOURCE

#include "monitoring_filter.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Case-insensitive substring matching                                       */
/* ------------------------------------------------------------------------- */

int filter_contains_ci(const char *text, const char *pattern) {
  size_t pattern_length;
  size_t i;

  if (text == NULL || pattern == NULL) {
    return 0;
  }

  if (*pattern == '\0') {
    return 1;
  }

  pattern_length = strlen(pattern);

  for (i = 0; text[i] != '\0'; ++i) {

    size_t j;

    for (j = 0; j < pattern_length; ++j) {

      if (text[i + j] == '\0') {
        break;
      }

      if (tolower((unsigned char)text[i + j]) !=
          tolower((unsigned char)pattern[j])) {
        break;
      }
    }

    if (j == pattern_length) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Include patterns                                                          */
/* ------------------------------------------------------------------------- */

int filter_match_patterns(const NeoConfig *config, const NeoProcess *process) {
  size_t i;

  if (config == NULL || process == NULL) {
    return 0;
  }

  /*
   * No include patterns means:
   *
   *     monitor everything
   *
   * This is the top-like default behavior.
   */
  if (config->pattern_count == 0) {
    return 1;
  }

  for (i = 0; i < config->pattern_count; ++i) {

    const char *pattern = config->patterns[i];

    if (pattern == NULL) {
      continue;
    }

    if (filter_contains_ci(process->comm, pattern)) {
      return 1;
    }

    if (filter_contains_ci(process->cmdline, pattern)) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Exclusion patterns                                                        */
/* ------------------------------------------------------------------------- */

int filter_match_excludes(const NeoConfig *config, const NeoProcess *process) {
  size_t i;

  if (config == NULL || process == NULL) {
    return 0;
  }

  for (i = 0; i < config->exclude_count; ++i) {

    const char *pattern = config->exclude_patterns[i];

    if (pattern == NULL) {
      continue;
    }

    if (filter_contains_ci(process->comm, pattern)) {
      return 1;
    }

    if (filter_contains_ci(process->cmdline, pattern)) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* PID filter                                                                */
/* ------------------------------------------------------------------------- */

int filter_match_pid(const NeoConfig *config, const NeoProcess *process) {
  size_t i;

  if (config == NULL || process == NULL) {
    return 0;
  }

  if (config->pid_count == 0) {
    return 1;
  }

  for (i = 0; i < config->pid_count; ++i) {

    if (config->pids[i] == process->pid) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* PPID filter                                                               */
/* ------------------------------------------------------------------------- */

int filter_match_ppid(const NeoConfig *config, const NeoProcess *process) {
  size_t i;

  if (config == NULL || process == NULL) {
    return 0;
  }

  if (config->ppid_count == 0) {
    return 1;
  }

  for (i = 0; i < config->ppid_count; ++i) {

    if (config->ppids[i] == process->ppid) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* User filter                                                               */
/* ------------------------------------------------------------------------- */

int filter_match_user(const NeoConfig *config, const NeoProcess *process) {
  if (config == NULL || process == NULL) {
    return 0;
  }

  if (config->filter_uid_enabled) {

    if (process->uid != config->filter_uid) {
      return 0;
    }
  }

  if (config->filter_user_enabled) {

    if (strcasecmp(process->user, config->filter_user) != 0) {
      return 0;
    }
  }

  return 1;
}

/* ------------------------------------------------------------------------- */
/* State filter                                                              */
/* ------------------------------------------------------------------------- */

int filter_match_state(const NeoConfig *config, const NeoProcess *process) {
  size_t i;

  if (config == NULL || process == NULL) {
    return 0;
  }

  if (config->state_count == 0) {
    return 1;
  }

  for (i = 0; i < config->state_count; ++i) {

    if (config->states[i] == process->state) {
      return 1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Complete filter                                                           */
/* ------------------------------------------------------------------------- */

int process_matches(const NeoConfig *config, const NeoProcess *process) {
  if (config == NULL || process == NULL) {
    return 0;
  }

  /*
   * Include pattern test.
   */
  if (!filter_match_patterns(config, process)) {
    return 0;
  }

  /*
   * Exclusions have priority.
   */
  if (filter_match_excludes(config, process)) {
    return 0;
  }

  /*
   * PID.
   */
  if (!filter_match_pid(config, process)) {
    return 0;
  }

  /*
   * PPID.
   */
  if (!filter_match_ppid(config, process)) {
    return 0;
  }

  /*
   * User / UID.
   */
  if (!filter_match_user(config, process)) {
    return 0;
  }

  /*
   * State.
   */
  if (!filter_match_state(config, process)) {
    return 0;
  }

  return 1;
}
