#define _GNU_SOURCE

#include "monitoring_remote.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Dynamic buffer                                                            */
/* ------------------------------------------------------------------------- */

#define CHUNK_SIZE 4096
#define DEFAULT_TIMEOUT_MS 20000

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} NeoDynBuffer;

static void dynbuf_init(NeoDynBuffer *buf) {
  buf->data = NULL;
  buf->length = 0;
  buf->capacity = 0;
}

static int dynbuf_append(NeoDynBuffer *buf, const char *chunk,
                             size_t chunk_len) {
  if (buf->length + chunk_len + 1 > buf->capacity) {

    size_t new_capacity = buf->capacity == 0 ? CHUNK_SIZE : buf->capacity * 2;

    while (new_capacity < buf->length + chunk_len + 1) {
      new_capacity *= 2;
    }

    char *tmp = realloc(buf->data, new_capacity);

    if (tmp == NULL) {
      return -1;
    }

    buf->data = tmp;
    buf->capacity = new_capacity;
  }

  memcpy(buf->data + buf->length, chunk, chunk_len);

  buf->length += chunk_len;
  buf->data[buf->length] = '\0';

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Message helper                                                            */
/* ------------------------------------------------------------------------- */

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

static long long now_ms(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

/* ------------------------------------------------------------------------- */
/* Process spawn/capture harness                                            */
/*                                                                            */
/* Runs argv[0] with the given arguments directly via execvp (no local      */
/* shell involved, so no shell-injection risk from host/user strings).      */
/* Optionally feeds `stdin_data` to the child's stdin (used by telnet's     */
/* scripted login) and enforces an overall wall-clock timeout, since        */
/* interactive protocols like telnet have no reliable "done" signal other   */
/* than the remote side closing the connection.                             */
/* ------------------------------------------------------------------------- */

static int run_command(char *const argv[], const char *stdin_data,
                           int timeout_ms, char **out_stdout,
                           char **out_stderr) {
  int stdout_pipe[2];
  int stderr_pipe[2];
  int stdin_pipe[2] = {-1, -1};

  pid_t pid;
  int status;
  int timed_out = 0;

  NeoDynBuffer stdout_buf;
  NeoDynBuffer stderr_buf;

  dynbuf_init(&stdout_buf);
  dynbuf_init(&stderr_buf);

  if (out_stdout != NULL) {
    *out_stdout = NULL;
  }

  if (out_stderr != NULL) {
    *out_stderr = NULL;
  }

  if (pipe(stdout_pipe) != 0) {
    return -1;
  }

  if (pipe(stderr_pipe) != 0) {
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    return -1;
  }

  if (stdin_data != NULL) {

    if (pipe(stdin_pipe) != 0) {
      close(stdout_pipe[0]);
      close(stdout_pipe[1]);
      close(stderr_pipe[0]);
      close(stderr_pipe[1]);
      return -1;
    }
  }

  pid = fork();

  if (pid < 0) {

    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    if (stdin_pipe[0] >= 0) {
      close(stdin_pipe[0]);
      close(stdin_pipe[1]);
    }

    return -1;
  }

  if (pid == 0) {

    /* Child */
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    if (stdin_pipe[0] >= 0) {

      close(stdin_pipe[1]);
      dup2(stdin_pipe[0], STDIN_FILENO);
      close(stdin_pipe[0]);

    } else {

      /*
       * No interactive input is expected; detach stdin so a
       * misbehaving client cannot block waiting on a terminal.
       */
      int null_fd = open("/dev/null", O_RDONLY);

      if (null_fd >= 0) {
        dup2(null_fd, STDIN_FILENO);
        close(null_fd);
      }
    }

    execvp(argv[0], argv);

    /* execvp only returns on failure. */
    _exit(127);
  }

  /* Parent */
  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  if (stdin_pipe[0] >= 0) {

    close(stdin_pipe[0]);

    /*
     * The scripted login sequence is small (well under a typical
     * 64 KB pipe buffer), so a single blocking write is safe here.
     */
    size_t remaining = strlen(stdin_data);
    const char *cursor = stdin_data;

    while (remaining > 0) {

      ssize_t written = write(stdin_pipe[1], cursor, remaining);

      if (written < 0) {

        if (errno == EINTR) {
          continue;
        }

        break;
      }

      cursor += written;
      remaining -= (size_t)written;
    }

    close(stdin_pipe[1]);
  }

  {
    struct pollfd fds[2];
    int open_fds = 2;
    long long deadline = now_ms() + (timeout_ms > 0 ? timeout_ms : DEFAULT_TIMEOUT_MS);

    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    while (open_fds > 0) {

      long long remaining_ms = deadline - now_ms();

      if (remaining_ms <= 0) {
        timed_out = 1;
        break;
      }

      int ready = poll(fds, 2, (int)remaining_ms);

      if (ready < 0) {

        if (errno == EINTR) {
          continue;
        }

        break;
      }

      if (ready == 0) {
        timed_out = 1;
        break;
      }

      for (int i = 0; i < 2; ++i) {

        if (fds[i].fd < 0) {
          continue;
        }

        if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {

          char chunk[CHUNK_SIZE];
          ssize_t bytes_read = read(fds[i].fd, chunk, sizeof(chunk));

          if (bytes_read > 0) {

            dynbuf_append(i == 0 ? &stdout_buf : &stderr_buf, chunk,
                             (size_t)bytes_read);

          } else {

            close(fds[i].fd);
            fds[i].fd = -1;
            --open_fds;
          }
        }
      }
    }
  }

  if (timed_out) {

    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);

    if (stdout_pipe[0] >= 0) {
      close(stdout_pipe[0]);
    }

    if (stderr_pipe[0] >= 0) {
      close(stderr_pipe[0]);
    }

    dynbuf_append(&stderr_buf, "\n[timed out waiting for remote command]",
                     40);

  } else {

    waitpid(pid, &status, 0);
  }

  if (out_stdout != NULL) {
    *out_stdout = (stdout_buf.data != NULL) ? stdout_buf.data : strdup("");
  } else {
    free(stdout_buf.data);
  }

  if (out_stderr != NULL) {
    *out_stderr = (stderr_buf.data != NULL) ? stderr_buf.data : strdup("");
  } else {
    free(stderr_buf.data);
  }

  if (timed_out) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  return -1;
}

/* ------------------------------------------------------------------------- */
/* CSV parsing (mirrors monitoring_output.c's output_csv() exactly)         */
/*                                                                            */
/* Every non-matching line (banners, shell prompts, echoed input, the CSV   */
/* header itself) is silently skipped rather than treated as an error, so   */
/* this same parser works for both clean SSH output and noisy telnet        */
/* sessions with login banners mixed in.                                    */
/* ------------------------------------------------------------------------- */

static int remote_list_add(NeoProcessList *list, const NeoProcess *process) {

  if (list->count >= list->capacity) {

    size_t new_capacity =
        list->capacity == 0 ? INITIAL_CAPACITY : list->capacity * 2;

    NeoProcess *tmp = realloc(list->items, new_capacity * sizeof(*tmp));

    if (tmp == NULL) {
      return -1;
    }

    list->items = tmp;
    list->capacity = new_capacity;
  }

  list->items[list->count] = *process;
  ++list->count;

  return 0;
}

static int parse_quoted_field(const char **cursor, char *out,
                                  size_t out_size) {
  const char *p = *cursor;
  size_t out_len = 0;

  if (*p != '"') {
    return -1;
  }

  ++p;

  while (*p != '\0') {

    if (*p == '"') {

      if (*(p + 1) == '"') {

        if (out_len + 1 < out_size) {
          out[out_len++] = '"';
        }

        p += 2;
        continue;
      }

      ++p;
      break;
    }

    if (out_len + 1 < out_size) {
      out[out_len++] = *p;
    }

    ++p;
  }

  out[out_len < out_size ? out_len : out_size - 1] = '\0';

  *cursor = p;

  return 0;
}

static int parse_csv_line(const char *line, NeoProcess *out) {
  const char *p = line;

  char field[15][64];
  int field_index = 0;
  size_t field_len = 0;

  memset(out, 0, sizeof(*out));
  memset(field, 0, sizeof(field));

  while (field_index < 15) {

    if (*p == ',' || *p == '\0') {

      field[field_index]
           [field_len < sizeof(field[0]) ? field_len : sizeof(field[0]) - 1] =
          '\0';

      ++field_index;
      field_len = 0;

      if (*p == '\0') {
        return -1; /* line ended before all fields were seen */
      }

      ++p;
      continue;
    }

    if (field_len + 1 < sizeof(field[0])) {
      field[field_index][field_len++] = *p;
    }

    ++p;
  }

  if (parse_quoted_field(&p, out->comm, sizeof(out->comm)) != 0) {
    return -1;
  }

  if (*p != ',') {
    return -1;
  }

  ++p;

  if (parse_quoted_field(&p, out->cmdline, sizeof(out->cmdline)) != 0) {
    return -1;
  }

  out->pid = (pid_t)strtol(field[0], NULL, 10);
  out->ppid = (pid_t)strtol(field[1], NULL, 10);

  snprintf(out->user, sizeof(out->user), "%s", field[2]);

  out->uid = (uid_t)strtoul(field[3], NULL, 10);
  out->state = field[4][0];
  out->threads = strtoul(field[5], NULL, 10);
  out->cpu_percent = strtod(field[6], NULL);
  out->mem_percent = strtod(field[7], NULL);
  out->rss_mb = strtod(field[8], NULL);
  out->vsz_mb = strtod(field[9], NULL);
  out->swap_mb = strtod(field[10], NULL);
  out->io_read_mb_s = strtod(field[11], NULL);
  out->io_write_mb_s = strtod(field[12], NULL);
  out->start_time = (time_t)strtoll(field[13], NULL, 10);
  out->elapsed_seconds = strtod(field[14], NULL);

  out->rss_kb = (unsigned long long)(out->rss_mb * 1024.0);
  out->vsz_kb = (unsigned long long)(out->vsz_mb * 1024.0);
  out->swap_kb = (unsigned long long)(out->swap_mb * 1024.0);

  if (out->pid <= 0) {
    return -1; /* reject accidental matches on noise lines */
  }

  return 0;
}

static int parse_csv_output(const char *csv_text, NeoProcessList *list) {
  char *copy;
  char *saveptr;
  char *line;

  if (csv_text == NULL || list == NULL) {
    return -1;
  }

  copy = strdup(csv_text);

  if (copy == NULL) {
    return -1;
  }

  process_list_free(list);
  process_list_init(list);

  line = strtok_r(copy, "\n", &saveptr);

  while (line != NULL) {

    size_t len = strlen(line);

    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
    }

    if (line[0] != '\0') {

      NeoProcess process;

      if (parse_csv_line(line, &process) == 0) {
        remote_list_add(list, &process);
      }

      /*
       * Anything that doesn't parse as a well-formed row (banners,
       * prompts, the CSV header) is silently skipped rather than
       * aborting the whole refresh.
       */
    }

    line = strtok_r(NULL, "\n", &saveptr);
  }

  free(copy);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Remote system info (bundled into the same session as the process CSV)    */
/* ------------------------------------------------------------------------- */

#define NEO_SYSINFO_MARKER "###NEO_SYSTEM_INFO_BEGIN###"
#define NEO_CSV_MARKER "###NEO_PROCESS_CSV_BEGIN###"

/*
 * Builds a single remote shell one-liner that prints total memory and
 * CPU count, then the process CSV, separated by markers - so one SSH
 * or telnet round trip gets both instead of two.
 */
static void build_combined_remote_command(const char *remote_binary,
                                              char *buffer,
                                              size_t buffer_size) {
  snprintf(buffer, buffer_size,
              "echo " NEO_SYSINFO_MARKER "; "
              "grep '^MemTotal:' /proc/meminfo; nproc; "
              "echo " NEO_CSV_MARKER "; "
              "%s --csv --once",
              remote_binary[0] ? remote_binary : "app_top_monitoring");
}

static int parse_remote_system_info(const char *text, NeoSystemInfo *out) {
  char *copy;
  char *saveptr;
  char *line;

  unsigned long long mem_kb = 0;
  unsigned int cpu_count = 0;
  int found_mem = 0;
  int found_cpu = 0;

  if (text == NULL || out == NULL) {
    return -1;
  }

  copy = strdup(text);

  if (copy == NULL) {
    return -1;
  }

  line = strtok_r(copy, "\n", &saveptr);

  while (line != NULL) {

    unsigned long long value;

    if (sscanf(line, "MemTotal: %llu kB", &value) == 1 ||
        sscanf(line, "MemTotal:%llu kB", &value) == 1) {

      mem_kb = value;
      found_mem = 1;

    } else if (!found_cpu) {

      char *endptr;
      long parsed = strtol(line, &endptr, 10);

      /* nproc's output is a single bare integer on its own line. */
      if (endptr != line && *endptr == '\0' && parsed > 0) {
        cpu_count = (unsigned int)parsed;
        found_cpu = 1;
      }
    }

    line = strtok_r(NULL, "\n", &saveptr);
  }

  free(copy);

  memset(out, 0, sizeof(*out));

  out->total_memory_kb = mem_kb;
  out->cpu_count = found_cpu ? cpu_count : 1;

  return (found_mem || found_cpu) ? 0 : -1;
}

/*
 * Like strstr(), but returns the last match instead of the first.
 * Needed because a telnet session typically echoes back the command
 * you typed - which itself contains the literal marker text as part
 * of `echo ###MARKER###; ...` - before the real output (containing
 * the genuine markers) appears further down the capture.
 */
static const char *find_last_occurrence(const char *haystack,
                                            const char *needle) {
  const char *result = NULL;
  const char *cursor = haystack;

  while ((cursor = strstr(cursor, needle)) != NULL) {
    result = cursor;
    ++cursor;
  }

  return result;
}

/*
 * Splits a captured session into its system-info and CSV sections
 * (using the markers above) and parses each. Uses the *last* match of
 * each marker so an echoed command line containing the marker text
 * doesn't get mistaken for the real output. Falls back to treating
 * the whole capture as CSV if the markers are missing or out of
 * order - e.g. an older remote binary - so a partial capture still
 * yields a process list even if the system totals can't be recovered.
 */
static int parse_combined_output(const char *text, NeoProcessList *list,
                                     NeoSystemInfo *system) {
  const char *sysinfo_start = find_last_occurrence(text, NEO_SYSINFO_MARKER);
  const char *csv_start = find_last_occurrence(text, NEO_CSV_MARKER);

  if (system != NULL) {
    memset(system, 0, sizeof(*system));
    system->cpu_count = 1;
  }

  if (sysinfo_start != NULL && csv_start != NULL && csv_start > sysinfo_start) {

    size_t sysinfo_len = (size_t)(csv_start - (sysinfo_start +
                                                   strlen(NEO_SYSINFO_MARKER)));
    char *sysinfo_block = malloc(sysinfo_len + 1);

    if (sysinfo_block != NULL) {

      memcpy(sysinfo_block, sysinfo_start + strlen(NEO_SYSINFO_MARKER),
                sysinfo_len);

      sysinfo_block[sysinfo_len] = '\0';

      if (system != NULL) {
        /* Best-effort: totals are a nice-to-have, not fatal if absent. */
        parse_remote_system_info(sysinfo_block, system);
      }

      free(sysinfo_block);
    }

    return parse_csv_output(csv_start + strlen(NEO_CSV_MARKER), list);
  }

  /*
   * Markers not found - fall back to parsing the entire capture as
   * CSV directly (same behavior as before system info was added).
   */
  return parse_csv_output(text, list);
}

/* ------------------------------------------------------------------------- */
/* Target initialization                                                    */
/* ------------------------------------------------------------------------- */

void remote_target_init(NeoRemoteTarget *target) {

  if (target == NULL) {
    return;
  }

  memset(target, 0, sizeof(*target));

  snprintf(target->remote_binary, sizeof(target->remote_binary),
           "app_top_monitoring");
}

void ftp_target_init(NeoFtpTarget *target) {

  if (target == NULL) {
    return;
  }

  memset(target, 0, sizeof(*target));

  target->port = 21;
}

void tftp_target_init(NeoTftpTarget *target) {

  if (target == NULL) {
    return;
  }

  memset(target, 0, sizeof(*target));

  target->port = 69;
}

/* ------------------------------------------------------------------------- */
/* SSH                                                                       */
/* ------------------------------------------------------------------------- */

static void build_user_host(const char *user, const char *host, char *buffer,
                                size_t buffer_size) {

  if (user != NULL && user[0] != '\0') {
    snprintf(buffer, buffer_size, "%s@%s", user, host);
  } else {
    snprintf(buffer, buffer_size, "%s", host);
  }
}

static int build_ssh_argv(const NeoRemoteTarget *target,
                              const char *remote_command, char **argv_buf,
                              int argv_capacity, char *user_host_buf,
                              size_t user_host_buf_size, char *port_buf,
                              size_t port_buf_size) {
  int argc = 0;

  build_user_host(target->user, target->host, user_host_buf,
                      user_host_buf_size);

  snprintf(port_buf, port_buf_size, "%d", target->port > 0 ? target->port : 22);

  if (argc + 12 > argv_capacity) {
    return -1;
  }

  argv_buf[argc++] = "ssh";
  argv_buf[argc++] = "-p";
  argv_buf[argc++] = port_buf;
  argv_buf[argc++] = "-o";
  argv_buf[argc++] = "BatchMode=yes";
  argv_buf[argc++] = "-o";
  argv_buf[argc++] = "ConnectTimeout=5";
  argv_buf[argc++] = "-o";
  argv_buf[argc++] = "StrictHostKeyChecking=accept-new";

  if (target->identity_file[0] != '\0') {
    argv_buf[argc++] = "-i";
    argv_buf[argc++] = (char *)target->identity_file;
  }

  argv_buf[argc++] = user_host_buf;
  argv_buf[argc++] = (char *)remote_command;
  argv_buf[argc++] = NULL;

  return argc;
}

int remote_ssh_scan(const NeoRemoteTarget *target, NeoProcessList *list,
                        NeoSystemInfo *system, char *message,
                        size_t message_size) {
  char user_host[REMOTE_HOST_MAX + REMOTE_USER_MAX + 2];
  char port_str[16];
  char remote_command[REMOTE_PATH_MAX + 128];
  char *argv_buf[16];

  char *out = NULL;
  char *err = NULL;
  int exit_code;

  if (target == NULL || list == NULL) {
    set_message(message, message_size, "Invalid arguments");
    return -1;
  }

  if (target->host[0] == '\0') {
    set_message(message, message_size, "Remote host not set");
    return -1;
  }

  build_combined_remote_command(target->remote_binary, remote_command,
                                    sizeof(remote_command));

  if (build_ssh_argv(target, remote_command, argv_buf, 16, user_host,
                         sizeof(user_host), port_str,
                         sizeof(port_str)) < 0) {
    set_message(message, message_size, "Internal error building ssh command");
    return -1;
  }

  exit_code = run_command(argv_buf, NULL, DEFAULT_TIMEOUT_MS, &out, &err);

  if (exit_code != 0) {
    set_message(message, message_size, "SSH command failed (exit %d): %s",
                   exit_code,
                   (err != NULL && err[0] != '\0') ? err : "no error output");
    free(out);
    free(err);
    return -1;
  }

  if (parse_combined_output(out, list, system) != 0) {
    set_message(message, message_size, "Failed to parse remote CSV output");
    free(out);
    free(err);
    return -1;
  }

  free(out);
  free(err);

  return 0;
}

int remote_ssh_check(const NeoRemoteTarget *target, bool *binary_found,
                         char *message, size_t message_size) {
  char user_host[REMOTE_HOST_MAX + REMOTE_USER_MAX + 2];
  char port_str[16];
  char remote_command[REMOTE_PATH_MAX + 96];
  char *argv_buf[16];

  char *out = NULL;
  char *err = NULL;
  int exit_code;

  if (binary_found != NULL) {
    *binary_found = false;
  }

  if (target == NULL) {
    set_message(message, message_size, "Invalid arguments");
    return -1;
  }

  if (target->host[0] == '\0') {
    set_message(message, message_size, "Remote host not set");
    return -1;
  }

  snprintf(remote_command, sizeof(remote_command),
           "command -v %s >/dev/null 2>&1 && echo NEO_BINARY_OK || "
           "echo NEO_BINARY_MISSING",
           target->remote_binary[0] ? target->remote_binary
                                     : "app_top_monitoring");

  if (build_ssh_argv(target, remote_command, argv_buf, 16, user_host,
                         sizeof(user_host), port_str,
                         sizeof(port_str)) < 0) {
    set_message(message, message_size, "Internal error building ssh command");
    return -1;
  }

  exit_code = run_command(argv_buf, NULL, DEFAULT_TIMEOUT_MS, &out, &err);

  if (exit_code != 0) {
    set_message(message, message_size,
                   "SSH connection failed (exit %d): %s", exit_code,
                   (err != NULL && err[0] != '\0') ? err : "no error output");
    free(out);
    free(err);
    return -1;
  }

  if (binary_found != NULL && out != NULL &&
      strstr(out, "NEO_BINARY_OK") != NULL) {
    *binary_found = true;
  }

  set_message(message, message_size, "Connected to %s (%s)", target->host,
                 (binary_found != NULL && *binary_found)
                     ? "monitoring tool found"
                     : "monitoring tool not found on remote");

  free(out);
  free(err);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Telnet                                                                    */
/*                                                                            */
/* telnet has no batch/non-interactive mode, so this scripts a login by     */
/* writing the whole session (username, password, command, exit) to the     */
/* client's stdin up front and then reading everything it produces until    */
/* the connection closes or the timeout elapses. This is best-effort: it    */
/* assumes a plain login prompt followed directly by a shell, with no       */
/* unusual terminal negotiation. Prefer SSH whenever the card supports it.  */
/* ------------------------------------------------------------------------- */

int remote_telnet_scan(const NeoRemoteTarget *target, NeoProcessList *list,
                           NeoSystemInfo *system, char *message,
                           size_t message_size) {
  char port_str[16];
  char remote_command[REMOTE_PATH_MAX + 128];
  char script[2200];
  size_t offset = 0;

  char *argv_buf[4];
  char *out = NULL;
  char *err = NULL;
  int exit_code;

  if (target == NULL || list == NULL) {
    set_message(message, message_size, "Invalid arguments");
    return -1;
  }

  if (target->host[0] == '\0') {
    set_message(message, message_size, "Remote host not set");
    return -1;
  }

  build_combined_remote_command(target->remote_binary, remote_command,
                                    sizeof(remote_command));

  if (target->user[0] != '\0') {
    offset += (size_t)snprintf(script + offset, sizeof(script) - offset,
                                   "%s\n", target->user);
  }

  if (target->password[0] != '\0') {
    offset += (size_t)snprintf(script + offset, sizeof(script) - offset,
                                   "%s\n", target->password);
  }

  offset += (size_t)snprintf(script + offset, sizeof(script) - offset, "%s\n",
                                 remote_command);

  snprintf(script + offset, sizeof(script) - offset, "exit\n");

  snprintf(port_str, sizeof(port_str), "%d", target->port > 0 ? target->port : 23);

  argv_buf[0] = "telnet";
  argv_buf[1] = (char *)target->host;
  argv_buf[2] = port_str;
  argv_buf[3] = NULL;

  exit_code = run_command(argv_buf, script, DEFAULT_TIMEOUT_MS, &out, &err);

  /*
   * A non-zero exit or a completely empty capture usually means the
   * TCP connection itself never came up (host down/unreachable/wrong
   * port). Genuine login failures show up as ordinary text in `out`
   * and are handled below by parse_combined_output() simply finding
   * no valid rows.
   */
  if (exit_code != 0 && (out == NULL || out[0] == '\0')) {
    set_message(message, message_size, "Telnet connection failed: %s",
                   (err != NULL && err[0] != '\0') ? err
                                                    : "no error output");
    free(out);
    free(err);
    return -1;
  }

  if (parse_combined_output(out, list, system) != 0) {
    set_message(message, message_size, "Failed to parse remote CSV output");
    free(out);
    free(err);
    return -1;
  }


  if (list->count == 0) {
    set_message(message, message_size,
                   "Connected, but no valid data was found in the telnet "
                   "session output. Check the login sequence and that "
                   "\"%s\" is on the card's PATH.",
                   target->remote_binary[0] ? target->remote_binary
                                             : "app_top_monitoring");
    free(out);
    free(err);
    return -1;
  }

  free(out);
  free(err);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* FTP (via curl)                                                           */
/* ------------------------------------------------------------------------- */

int remote_ftp_deploy(const NeoFtpTarget *target, const char *local_path,
                          char *message, size_t message_size) {
  char url[REMOTE_HOST_MAX + REMOTE_USER_MAX * 2 + REMOTE_PATH_MAX + 32];
  char *argv_buf[8];
  char *out = NULL;
  char *err = NULL;
  int exit_code;
  const char *remote_name;

  if (target == NULL || local_path == NULL) {
    set_message(message, message_size, "Invalid arguments");
    return -1;
  }

  if (target->host[0] == '\0') {
    set_message(message, message_size, "FTP host not set");
    return -1;
  }

  if (access(local_path, R_OK) != 0) {
    set_message(message, message_size, "Local file not readable: %s",
                   local_path);
    return -1;
  }

  remote_name = target->remote_filename[0] != '\0'
                    ? target->remote_filename
                    : local_path;

  if (target->user[0] != '\0') {
    snprintf(url, sizeof(url), "ftp://%s:%s@%s:%d/%s", target->user,
                target->password, target->host,
                target->port > 0 ? target->port : 21, remote_name);
  } else {
    snprintf(url, sizeof(url), "ftp://%s:%d/%s", target->host,
                target->port > 0 ? target->port : 21, remote_name);
  }

  argv_buf[0] = "curl";
  argv_buf[1] = "-sS";
  argv_buf[2] = "-T";
  argv_buf[3] = (char *)local_path;
  argv_buf[4] = url;
  argv_buf[5] = NULL;

  exit_code = run_command(argv_buf, NULL, DEFAULT_TIMEOUT_MS, &out, &err);

  if (exit_code != 0) {
    set_message(message, message_size, "FTP upload failed: %s",
                   (err != NULL && err[0] != '\0') ? err : "unknown error");
    free(out);
    free(err);
    return -1;
  }

  free(out);
  free(err);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* TFTP                                                                      */
/* ------------------------------------------------------------------------- */

int remote_tftp_deploy(const NeoTftpTarget *target, const char *local_path,
                           char *message, size_t message_size) {
  char port_str[16];
  char *argv_buf[12];
  int argc = 0;

  char *out = NULL;
  char *err = NULL;
  int exit_code;

  if (target == NULL || local_path == NULL) {
    set_message(message, message_size, "Invalid arguments");
    return -1;
  }

  if (target->host[0] == '\0') {
    set_message(message, message_size, "TFTP host not set");
    return -1;
  }

  if (access(local_path, R_OK) != 0) {
    set_message(message, message_size, "Local file not readable: %s",
                   local_path);
    return -1;
  }

  snprintf(port_str, sizeof(port_str), "%d", target->port > 0 ? target->port : 69);

  argv_buf[argc++] = "tftp";
  argv_buf[argc++] = "-m";
  argv_buf[argc++] = "binary";
  argv_buf[argc++] = (char *)target->host;
  argv_buf[argc++] = port_str;
  argv_buf[argc++] = "-c";
  argv_buf[argc++] = "put";
  argv_buf[argc++] = (char *)local_path;
  argv_buf[argc++] =
      (char *)(target->remote_filename[0] ? target->remote_filename
                                              : local_path);
  argv_buf[argc++] = NULL;

  exit_code = run_command(argv_buf, NULL, DEFAULT_TIMEOUT_MS, &out, &err);

  /*
   * Several tftp client implementations (notably tftp-hpa) return
   * exit code 0 even when the transfer failed, so failures are also
   * detected by scanning the combined output for known error markers.
   */
  {
    const int looks_like_error =
        (out != NULL &&
         (strstr(out, "Error") != NULL || strstr(out, "error") != NULL ||
          strstr(out, "timed out") != NULL)) ||
        (err != NULL && err[0] != '\0');

    if (exit_code != 0 || looks_like_error) {

      set_message(message, message_size, "TFTP transfer failed: %s",
                     (out != NULL && out[0] != '\0')
                         ? out
                         : (err != NULL && err[0] != '\0') ? err
                                                            : "unknown error");
      free(out);
      free(err);
      return -1;
    }
  }

  free(out);
  free(err);

  return 0;
}
