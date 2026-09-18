#define _GNU_SOURCE

#include "monitoring_output.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static const char *state_name(char state) {
  switch (state) {
  case 'R':
    return "RUN";
  case 'S':
    return "SLEEP";
  case 'D':
    return "DISK";
  case 'T':
    return "STOP";
  case 't':
    return "TRACE";
  case 'Z':
    return "ZOMB";
  case 'X':
    return "DEAD";
  case 'I':
    return "IDLE";
  default:
    return "?";
  }
}

static void print_header(const NeoConfig *config) {
  printf("%-7s %-7s %-10s %-5s "
         "%6s %6s %8s %8s",
         "PID", "PPID", "USER", "STAT", "CPU%", "MEM%", "RSS", "VSZ");

  if (config->show_swap) {
    printf(" %8s", "SWAP");
  }

  if (config->show_io) {
    printf(" %10s %10s", "READ/s", "WRITE/s");
  }

  if (config->show_threads) {
    printf(" %7s", "THREADS");
  }

  if (config->show_start_time) {
    printf(" %-19s", "START");
  }

  if (config->show_elapsed) {
    printf(" %9s", "ELAPSED");
  }

  printf("  %s\n", config->show_long_args ? "COMMAND" : "NAME");

  printf("------- ------- ---------- ----- "
         "------ ------ -------- --------");

  if (config->show_swap) {
    printf(" --------");
  }

  if (config->show_io) {
    printf(" ---------- ----------");
  }

  if (config->show_threads) {
    printf(" -------");
  }

  if (config->show_start_time) {
    printf(" -------------------");
  }

  if (config->show_elapsed) {
    printf(" ---------");
  }

  printf("  -------\n");
}

static void format_elapsed(double seconds, char *buffer, size_t size) {
  unsigned long total;
  unsigned long days;
  unsigned long hours;
  unsigned long minutes;
  unsigned long secs;

  if (buffer == NULL || size == 0) {
    return;
  }

  if (seconds < 0.0) {
    seconds = 0.0;
  }

  total = (unsigned long)seconds;

  days = total / 86400UL;
  total %= 86400UL;

  hours = total / 3600UL;
  total %= 3600UL;

  minutes = total / 60UL;
  secs = total % 60UL;

  if (days > 0) {
    snprintf(buffer, size, "%lud %02lu:%02lu:%02lu", days, hours, minutes,
             secs);
  } else {
    snprintf(buffer, size, "%02lu:%02lu:%02lu", hours, minutes, secs);
  }
}

static void format_start_time(time_t start_time, char *buffer, size_t size) {
  struct tm tm_value;

  if (buffer == NULL || size == 0) {
    return;
  }

  if (start_time <= 0) {
    snprintf(buffer, size, "-");
    return;
  }

  if (localtime_r(&start_time, &tm_value) == NULL) {
    snprintf(buffer, size, "-");
    return;
  }

  strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &tm_value);
}

/* ------------------------------------------------------------------------- */
/* Process table row                                                         */
/* ------------------------------------------------------------------------- */

void output_process_row(const NeoConfig *config, const NeoProcess *process) {
  char elapsed[32];
  char start_time[32];

  const char *command;

  if (config == NULL || process == NULL) {
    return;
  }

  command = config->show_long_args ? process->cmdline : process->comm;

  format_elapsed(process->elapsed_seconds, elapsed, sizeof(elapsed));

  format_start_time(process->start_time, start_time, sizeof(start_time));

  printf("%-7ld %-7ld %-10.10s %-5s "
         "%6.2f %6.2f %7.1fM %7.1fM",
         (long)process->pid, (long)process->ppid, process->user,
         state_name(process->state), process->cpu_percent, process->mem_percent,
         process->rss_mb, process->vsz_mb);

  if (config->show_swap) {
    printf(" %7.1fM", process->swap_mb);
  }

  if (config->show_io) {
    printf(" %9.2fM %9.2fM", process->io_read_mb_s, process->io_write_mb_s);
  }

  if (config->show_threads) {
    printf(" %7lu", process->threads);
  }

  if (config->show_start_time) {
    printf(" %-19s", start_time);
  }

  if (config->show_elapsed) {
    printf(" %9s", elapsed);
  }

  printf("  %s\n", command);
}

/* ------------------------------------------------------------------------- */
/* Table output                                                              */
/* ------------------------------------------------------------------------- */

void output_table(const NeoConfig *config, const NeoSystemInfo *system,
                  const NeoProcessList *list) {
  size_t i;
  size_t count;

  if (config == NULL || system == NULL || list == NULL) {
    return;
  }

  /*
   * Header.
   */
  printf("monitoring_services %s\n", VERSION);

  printf("Processes: %zu | CPUs: %u | "
         "Memory: %.1f MB | Refresh: %.2fs\n",
         list->count, system->cpu_count,
         (double)system->total_memory_kb / 1024.0, config->interval);

  printf("Sort: ");

  switch (config->sort_mode) {
  case SORT_CPU:
    printf("CPU");
    break;

  case SORT_MEM:
    printf("MEMORY");
    break;

  case SORT_PID:
    printf("PID");
    break;

  case SORT_RSS:
    printf("RSS");
    break;

  case SORT_IO_READ:
    printf("IO-READ");
    break;

  case SORT_IO_WRITE:
    printf("IO-WRITE");
    break;

  default:
    printf("UNKNOWN");
    break;
  }

  if (config->reverse) {
    printf(" (reverse)");
  }

  printf(" | Keys: q=quit c=cpu m=mem p=pid +/-=interval r=refresh\n");

  putchar('\n');

  print_header(config);

  count = list->count;

  if (config->limit > 0 && count > config->limit) {
    count = config->limit;
  }

  for (i = 0; i < count; ++i) {
    output_process_row(config, &list->items[i]);
  }

  fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* JSON escaping                                                             */
/* ------------------------------------------------------------------------- */

void output_json_string(const char *text) {
  const unsigned char *p;

  putchar('"');

  if (text != NULL) {

    for (p = (const unsigned char *)text; *p != '\0'; ++p) {

      switch (*p) {

      case '"':
        fputs("\\\"", stdout);
        break;

      case '\\':
        fputs("\\\\", stdout);
        break;

      case '\b':
        fputs("\\b", stdout);
        break;

      case '\f':
        fputs("\\f", stdout);
        break;

      case '\n':
        fputs("\\n", stdout);
        break;

      case '\r':
        fputs("\\r", stdout);
        break;

      case '\t':
        fputs("\\t", stdout);
        break;

      default:
        if (*p < 0x20) {
          printf("\\u%04x", (unsigned int)*p);
        } else {
          putchar(*p);
        }
        break;
      }
    }
  }

  putchar('"');
}

/* ------------------------------------------------------------------------- */
/* CSV header                                                                */
/* ------------------------------------------------------------------------- */

void output_csv_header(void) {
  printf("pid,ppid,user,uid,state,threads,"
         "cpu_percent,mem_percent,rss_mb,vsz_mb,swap_mb,"
         "io_read_mb_s,io_write_mb_s,"
         "start_time,elapsed_seconds,comm,cmdline\n");
}

/* ------------------------------------------------------------------------- */
/* CSV output                                                                */
/* ------------------------------------------------------------------------- */

void output_csv(const NeoConfig *config, const NeoProcessList *list) {
  size_t i;
  size_t count;

  if (config == NULL || list == NULL) {
    return;
  }

  output_csv_header();

  count = list->count;

  if (config->limit > 0 && count > config->limit) {
    count = config->limit;
  }

  for (i = 0; i < count; ++i) {

    const NeoProcess *p = &list->items[i];

    /*
     * CSV values are kept numeric wherever possible.
     * Strings are quoted and internal quotes are doubled.
     */
    printf("%ld,%ld,%s,%u,%c,%lu,"
           "%.4f,%.4f,%.4f,%.4f,%.4f,"
           "%.4f,%.4f,%lld,%.3f,",
           (long)p->pid, (long)p->ppid, p->user, (unsigned int)p->uid, p->state,
           p->threads, p->cpu_percent, p->mem_percent, p->rss_mb, p->vsz_mb,
           p->swap_mb, p->io_read_mb_s, p->io_write_mb_s,
           (long long)p->start_time, p->elapsed_seconds);

    printf("\"");

    {
      const char *s = p->comm;

      while (*s != '\0') {

        if (*s == '"') {
          fputs("\"\"", stdout);
        } else {
          putchar(*s);
        }

        ++s;
      }
    }

    printf("\",");

    printf("\"");

    {
      const char *s = p->cmdline;

      while (*s != '\0') {

        if (*s == '"') {
          fputs("\"\"", stdout);
        } else {
          putchar(*s);
        }

        ++s;
      }
    }

    printf("\"\n");
  }

  fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* JSON output                                                               */
/* ------------------------------------------------------------------------- */

void output_json(const NeoConfig *config, const NeoSystemInfo *system,
                 const NeoProcessList *list) {
  size_t i;
  size_t count;

  if (config == NULL || system == NULL || list == NULL) {
    return;
  }

  count = list->count;

  if (config->limit > 0 && count > config->limit) {
    count = config->limit;
  }

  printf("{\n");

  printf("  \"version\": ");
  output_json_string(VERSION);
  printf(",\n");

  printf("  \"cpu_count\": %u,\n", system->cpu_count);

  printf("  \"total_memory_kb\": %llu,\n", system->total_memory_kb);

  printf("  \"process_count\": %zu,\n", count);

  printf("  \"processes\": [\n");

  for (i = 0; i < count; ++i) {

    const NeoProcess *p = &list->items[i];

    printf("    {\n");

    printf("      \"pid\": %ld,\n", (long)p->pid);

    printf("      \"ppid\": %ld,\n", (long)p->ppid);

    printf("      \"uid\": %u,\n", (unsigned int)p->uid);

    printf("      \"user\": ");
    output_json_string(p->user);
    printf(",\n");

    printf("      \"state\": ");
    output_json_string(p->comm);
    printf(",\n");

    /*
     * Keep the actual process state separate from the command name.
     */
    printf("      \"process_state\": \"%c\",\n", p->state);

    printf("      \"threads\": %lu,\n", p->threads);

    printf("      \"cpu_percent\": %.4f,\n", p->cpu_percent);

    printf("      \"memory_percent\": %.4f,\n", p->mem_percent);

    printf("      \"rss_mb\": %.4f,\n", p->rss_mb);

    printf("      \"vsz_mb\": %.4f,\n", p->vsz_mb);

    printf("      \"swap_mb\": %.4f,\n", p->swap_mb);

    printf("      \"io_read_mb_s\": %.4f,\n", p->io_read_mb_s);

    printf("      \"io_write_mb_s\": %.4f,\n", p->io_write_mb_s);

    printf("      \"start_time\": %lld,\n", (long long)p->start_time);

    printf("      \"elapsed_seconds\": %.3f,\n", p->elapsed_seconds);

    printf("      \"comm\": ");
    output_json_string(p->comm);
    printf(",\n");

    printf("      \"cmdline\": ");
    output_json_string(p->cmdline);
    printf("\n");

    if (i + 1 < count) {
      printf("    },\n");
    } else {
      printf("    }\n");
    }
  }

  printf("  ]\n");
  printf("}\n");

  fflush(stdout);
}
