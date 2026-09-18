#define _GNU_SOURCE

#include "monitoring_process.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static int read_file(const char *path, char *buffer, size_t size) {
  int fd;
  ssize_t total;

  if (path == NULL || buffer == NULL || size < 2) {
    return -1;
  }

  fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  total = read(fd, buffer, size - 1);

  close(fd);

  if (total <= 0) {
    return -1;
  }

  buffer[total] = '\0';

  return 0;
}

int parse_pid_path(const char *name, pid_t *pid) {
  char *endptr;
  long value;

  if (name == NULL || pid == NULL || *name == '\0') {
    return -1;
  }

  errno = 0;
  value = strtol(name, &endptr, 10);

  if (errno != 0 || endptr == name || *endptr != '\0') {
    return -1;
  }

  if (value <= 0) {
    return -1;
  }

  *pid = (pid_t)value;

  return 0;
}

/* ------------------------------------------------------------------------- */
/* /proc/<pid>/cmdline                                                       */
/* ------------------------------------------------------------------------- */

int process_read_cmdline(pid_t pid, char *buffer, size_t size) {
  char path[64];
  char raw[MAX_CMDLINE];
  ssize_t length;
  size_t i;
  size_t out = 0;

  if (buffer == NULL || size == 0 || pid <= 0) {
    return -1;
  }

  buffer[0] = '\0';

  snprintf(path, sizeof(path), "/proc/%ld/cmdline", (long)pid);

  int fd = open(path, O_RDONLY | O_CLOEXEC);

  if (fd < 0) {
    return -1;
  }

  length = read(fd, raw, sizeof(raw) - 1);

  close(fd);

  if (length <= 0) {
    return -1;
  }

  raw[length] = '\0';

  for (i = 0; i < (size_t)length && out + 1 < size; ++i) {

    if (raw[i] == '\0') {

      if (out > 0 && buffer[out - 1] != ' ') {
        buffer[out++] = ' ';
      }

    } else {

      buffer[out++] = raw[i];
    }
  }

  while (out > 0 && buffer[out - 1] == ' ') {
    --out;
  }

  buffer[out] = '\0';

  return 0;
}

/* ------------------------------------------------------------------------- */
/* /proc/<pid>/io                                                            */
/* ------------------------------------------------------------------------- */

int process_read_io(pid_t pid, unsigned long long *read_bytes,
                    unsigned long long *write_bytes) {
  char path[64];
  FILE *fp;
  char line[256];

  unsigned long long read_value = 0;
  unsigned long long write_value = 0;

  if (pid <= 0 || read_bytes == NULL || write_bytes == NULL) {
    return -1;
  }

  snprintf(path, sizeof(path), "/proc/%ld/io", (long)pid);

  fp = fopen(path, "r");

  if (fp == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {

    unsigned long long value;

    if (sscanf(line, "read_bytes: %llu", &value) == 1) {
      read_value = value;
    }

    if (sscanf(line, "write_bytes: %llu", &value) == 1) {
      write_value = value;
    }
  }

  fclose(fp);

  *read_bytes = read_value;
  *write_bytes = write_value;

  return 0;
}

/* ------------------------------------------------------------------------- */
/* /proc/<pid>/status                                                        */
/* ------------------------------------------------------------------------- */

int process_read_status(pid_t pid, uid_t *uid, char *user, size_t user_size,
                        unsigned long long *rss_kb, unsigned long long *swap_kb,
                        unsigned long *threads) {
  char path[64];
  FILE *fp;
  char line[512];

  unsigned long real_uid = 0;
  unsigned long long rss = 0;
  unsigned long long swap = 0;
  unsigned long thread_count = 0;

  if (pid <= 0 || uid == NULL || user == NULL || user_size == 0 ||
      rss_kb == NULL || swap_kb == NULL || threads == NULL) {
    return -1;
  }

  snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);

  fp = fopen(path, "r");

  if (fp == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {

    unsigned long a;
    unsigned long b;
    unsigned long c;
    unsigned long d;

    unsigned long long value;

    if (sscanf(line, "Uid:\t%lu\t%lu\t%lu\t%lu", &a, &b, &c, &d) == 4) {
      real_uid = a;
      continue;
    }

    if (sscanf(line, "VmRSS: %llu kB", &value) == 1) {
      rss = value;
      continue;
    }

    if (sscanf(line, "VmSwap: %llu kB", &value) == 1) {
      swap = value;
      continue;
    }

    if (sscanf(line, "Threads: %lu", &thread_count) == 1) {
      continue;
    }
  }

  fclose(fp);

  *uid = (uid_t)real_uid;
  *rss_kb = rss;
  *swap_kb = swap;
  *threads = thread_count;

  {
    struct passwd pwd;
    struct passwd *result = NULL;
    char pwd_buffer[4096];

    if (getpwuid_r(*uid, &pwd, pwd_buffer, sizeof(pwd_buffer), &result) == 0 &&
        result != NULL && result->pw_name != NULL) {

      snprintf(user, user_size, "%s", result->pw_name);

    } else {

      snprintf(user, user_size, "%u", (unsigned int)*uid);
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* /proc/<pid>/stat                                                          */
/* ------------------------------------------------------------------------- */

int process_read_stat(pid_t pid, NeoProcess *process) {
  char path[64];
  FILE *fp;
  char buffer[8192];

  char *left_paren;
  char *right_paren;
  char *fields;
  char *saveptr;
  char *token;

  int field;

  unsigned long long utime = 0;
  unsigned long long stime = 0;
  unsigned long long vsize = 0;
  long long rss_pages = 0;

  if (pid <= 0 || process == NULL) {
    return -1;
  }

  snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);

  fp = fopen(path, "r");

  if (fp == NULL) {
    return -1;
  }

  if (fgets(buffer, sizeof(buffer), fp) == NULL) {
    fclose(fp);
    return -1;
  }

  fclose(fp);

  /*
   * /proc/<pid>/stat format starts with:
   *
   * pid (comm) state ppid ...
   *
   * The comm field can contain spaces and ')' characters, so we locate
   * the final ')' before parsing the remaining fields.
   */
  left_paren = strchr(buffer, '(');
  right_paren = strrchr(buffer, ')');

  if (left_paren == NULL || right_paren == NULL || right_paren <= left_paren) {
    return -1;
  }

  {
    size_t comm_length = (size_t)(right_paren - left_paren - 1);

    if (comm_length >= sizeof(process->comm)) {
      comm_length = sizeof(process->comm) - 1;
    }

    memcpy(process->comm, left_paren + 1, comm_length);

    process->comm[comm_length] = '\0';
  }

  /*
   * Everything after ')' starts at field 3.
   */
  fields = right_paren + 2;

  field = 3;

  token = strtok_r(fields, " ", &saveptr);

  while (token != NULL) {

    switch (field) {

    case 3:
      process->state = token[0];
      break;

    case 4:
      process->ppid = (pid_t)strtol(token, NULL, 10);
      break;

    case 14:
      utime = strtoull(token, NULL, 10);
      break;

    case 15:
      stime = strtoull(token, NULL, 10);
      break;

    case 20:
      process->threads = strtoul(token, NULL, 10);
      break;

    case 22:
      process->start_time = 0;
      break;

    case 23:
      vsize = strtoull(token, NULL, 10);
      break;

    case 24:
      rss_pages = strtoll(token, NULL, 10);
      break;

    default:
      break;
    }

    ++field;
    token = strtok_r(NULL, " ", &saveptr);
  }

  process->utime = utime;
  process->stime = stime;
  process->total_time = utime + stime;

  process->vsz_kb = vsize / 1024ULL;

  {
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size > 0 && rss_pages > 0) {
      process->rss_kb =
          ((unsigned long long)rss_pages * (unsigned long long)page_size) /
          1024ULL;
    } else {
      process->rss_kb = 0;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Process start time                                                        */
/* ------------------------------------------------------------------------- */

int process_get_start_time(pid_t pid, time_t *start_time) {
  char path[64];
  FILE *fp;
  char buffer[8192];

  char *left_paren;
  char *right_paren;
  char *fields;
  char *saveptr;
  char *token;

  int field = 3;
  unsigned long long start_ticks = 0;

  long ticks_per_second;
  FILE *stat_fp;

  char stat_line[512];
  unsigned long long boot_time = 0;

  if (pid <= 0 || start_time == NULL) {
    return -1;
  }

  snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);

  fp = fopen(path, "r");

  if (fp == NULL) {
    return -1;
  }

  if (fgets(buffer, sizeof(buffer), fp) == NULL) {
    fclose(fp);
    return -1;
  }

  fclose(fp);

  left_paren = strchr(buffer, '(');
  right_paren = strrchr(buffer, ')');

  if (left_paren == NULL || right_paren == NULL) {
    return -1;
  }

  fields = right_paren + 2;

  token = strtok_r(fields, " ", &saveptr);

  while (token != NULL) {

    if (field == 22) {
      start_ticks = strtoull(token, NULL, 10);
      break;
    }

    ++field;
    token = strtok_r(NULL, " ", &saveptr);
  }

  ticks_per_second = sysconf(_SC_CLK_TCK);

  if (ticks_per_second <= 0) {
    return -1;
  }

  /*
   * Read system boot time from /proc/stat.
   */
  stat_fp = fopen("/proc/stat", "r");

  if (stat_fp == NULL) {
    return -1;
  }

  while (fgets(stat_line, sizeof(stat_line), stat_fp) != NULL) {

    if (sscanf(stat_line, "btime %llu", &boot_time) == 1) {
      break;
    }
  }

  fclose(stat_fp);

  if (boot_time == 0) {
    return -1;
  }

  *start_time = (time_t)(boot_time +
                         (start_ticks / (unsigned long long)ticks_per_second));

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Complete process collection                                               */
/* ------------------------------------------------------------------------- */

int process_collect(pid_t pid, NeoProcess *process) {
  if (pid <= 0 || process == NULL) {
    return -1;
  }

  memset(process, 0, sizeof(*process));

  process->pid = pid;

  /*
   * stat contains command name, state, PPID, CPU time,
   * VSZ and RSS.
   */
  if (process_read_stat(pid, process) != 0) {
    return -1;
  }

  /*
   * status contains UID, username, RSS, swap and thread count.
   */
  if (process_read_status(pid, &process->uid, process->user,
                          sizeof(process->user), &process->rss_kb,
                          &process->swap_kb, &process->threads) != 0) {
    return -1;
  }

  /*
   * cmdline can legitimately fail for some kernel/system processes.
   * Fall back to the command name.
   */
  if (process_read_cmdline(pid, process->cmdline, sizeof(process->cmdline)) !=
      0) {

    snprintf(process->cmdline, sizeof(process->cmdline), "%s", process->comm);
  }

  /*
   * I/O statistics may not be readable for every process.
   * Keep zero values in that case.
   */
  if (process_read_io(pid, &process->read_bytes, &process->write_bytes) != 0) {

    process->read_bytes = 0;
    process->write_bytes = 0;
  }

  /*
   * Convert kernel start time into Unix time.
   */
  (void)process_get_start_time(pid, &process->start_time);

  return 0;
}
