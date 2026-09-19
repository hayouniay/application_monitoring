#define _GNU_SOURCE

#include "monitoring_filter.h"
#include "monitoring_metrics.h"
#include "monitoring_process.h"
#include "monitoring_services.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Internal helpers                                                          */
/* ------------------------------------------------------------------------- */

static int append_pid(pid_t **array, size_t *count, pid_t pid) {
  pid_t *tmp;

  if (array == NULL || count == NULL) {
    return -1;
  }

  tmp = realloc(*array, (*count + 1) * sizeof(**array));

  if (tmp == NULL) {
    return -1;
  }

  tmp[*count] = pid;

  *array = tmp;
  ++(*count);

  return 0;
}

static int append_pattern(char ***array, size_t *count, const char *pattern) {
  char **tmp;
  char *copy;

  if (array == NULL || count == NULL || pattern == NULL) {
    return -1;
  }

  copy = strdup(pattern);

  if (copy == NULL) {
    return -1;
  }

  tmp = realloc(*array, (*count + 1) * sizeof(**array));

  if (tmp == NULL) {
    free(copy);
    return -1;
  }

  tmp[*count] = copy;

  *array = tmp;
  ++(*count);

  return 0;
}

static int process_list_reserve(NeoProcessList *list, size_t capacity) {
  NeoProcess *tmp;

  if (list == NULL) {
    return -1;
  }

  if (capacity <= list->capacity) {
    return 0;
  }

  tmp = realloc(list->items, capacity * sizeof(*tmp));

  if (tmp == NULL) {
    return -1;
  }

  list->items = tmp;
  list->capacity = capacity;

  return 0;
}

static int process_list_add(NeoProcessList *list, const NeoProcess *process) {
  size_t new_capacity;

  if (list == NULL || process == NULL) {
    return -1;
  }

  if (list->count >= list->capacity) {

    if (list->capacity == 0) {
      new_capacity = INITIAL_CAPACITY;
    } else {
      new_capacity = list->capacity * 2;
    }

    if (process_list_reserve(list, new_capacity) != 0) {
      return -1;
    }
  }

  list->items[list->count] = *process;
  ++list->count;

  return 0;
}

static int previous_find(const NeoPreviousList *list, pid_t pid) {
  size_t i;

  if (list == NULL) {
    return -1;
  }

  for (i = 0; i < list->count; ++i) {

    if (list->items[i].pid == pid) {
      return (int)i;
    }
  }

  return -1;
}

static int previous_set(NeoPreviousList *list, const NeoProcess *process) {
  int index;
  NeoPreviousSample *tmp;

  if (list == NULL || process == NULL) {
    return -1;
  }

  index = previous_find(list, process->pid);

  if (index >= 0) {

    list->items[index].total_time = process->total_time;

    list->items[index].read_bytes = process->read_bytes;

    list->items[index].write_bytes = process->write_bytes;

    return 0;
  }

  tmp = realloc(list->items, (list->count + 1) * sizeof(*tmp));

  if (tmp == NULL) {
    return -1;
  }

  list->items = tmp;

  list->items[list->count].pid = process->pid;

  list->items[list->count].total_time = process->total_time;

  list->items[list->count].read_bytes = process->read_bytes;

  list->items[list->count].write_bytes = process->write_bytes;

  ++list->count;

  return 0;
}

static unsigned long long read_system_cpu(void) {
  FILE *fp;
  char line[1024];

  unsigned long long user = 0;
  unsigned long long nice = 0;
  unsigned long long system = 0;
  unsigned long long idle = 0;
  unsigned long long iowait = 0;
  unsigned long long irq = 0;
  unsigned long long softirq = 0;
  unsigned long long steal = 0;

  fp = fopen("/proc/stat", "r");

  if (fp == NULL) {
    return 0;
  }

  if (fgets(line, sizeof(line), fp) != NULL) {

    (void)sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user,
                 &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
  }

  fclose(fp);

  return user + nice + system + idle + iowait + irq + softirq + steal;
}

static int read_total_memory(unsigned long long *memory_kb) {
  FILE *fp;
  char line[512];

  unsigned long long value;

  if (memory_kb == NULL) {
    return -1;
  }

  *memory_kb = 0;

  fp = fopen("/proc/meminfo", "r");

  if (fp == NULL) {
    return -1;
  }

  while (fgets(line, sizeof(line), fp) != NULL) {

    if (sscanf(line, "MemTotal: %llu kB", &value) == 1) {

      *memory_kb = value;
      break;
    }
  }

  fclose(fp);

  return *memory_kb > 0 ? 0 : -1;
}

/* ------------------------------------------------------------------------- */
/* Configuration initialization                                              */
/* ------------------------------------------------------------------------- */

void config_init(NeoConfig *config) {
  if (config == NULL) {
    return;
  }

  memset(config, 0, sizeof(*config));

  config->interval = DEFAULT_INTERVAL;

  config->cpu_threshold = 0.0;
  config->ram_threshold_mb = 0.0;
  config->ram_threshold_percent = 0.0;

  config->show_long_args = false;
  config->show_vsz = true;
  config->show_swap = false;
  config->show_io = false;
  config->show_threads = false;
  config->show_start_time = false;
  config->show_elapsed = false;

  config->once = false;
  config->batch = false;
  config->csv = false;
  config->json = false;
  config->no_color = false;
  config->reverse = false;

  config->limit = 0;

  config->filter_uid = 0;
  config->filter_uid_enabled = false;

  config->filter_user[0] = '\0';
  config->filter_user_enabled = false;

  config->sort_mode = SORT_CPU;

  config->protocol = PROTOCOL_LOCAL;
  config->remote_port = 0;

  snprintf(config->remote_binary, sizeof(config->remote_binary),
           "app_top_monitoring");
}

/* ------------------------------------------------------------------------- */
/* Configuration cleanup                                                     */
/* ------------------------------------------------------------------------- */

void config_free(NeoConfig *config) {
  size_t i;

  if (config == NULL) {
    return;
  }

  free(config->pids);
  config->pids = NULL;
  config->pid_count = 0;

  free(config->ppids);
  config->ppids = NULL;
  config->ppid_count = 0;

  if (config->patterns != NULL) {

    for (i = 0; i < config->pattern_count; ++i) {
      free(config->patterns[i]);
    }

    free(config->patterns);
  }

  config->patterns = NULL;
  config->pattern_count = 0;

  if (config->exclude_patterns != NULL) {

    for (i = 0; i < config->exclude_count; ++i) {
      free(config->exclude_patterns[i]);
    }

    free(config->exclude_patterns);
  }

  config->exclude_patterns = NULL;
  config->exclude_count = 0;
}

/* ------------------------------------------------------------------------- */
/* Parse PID list                                                             */
/* ------------------------------------------------------------------------- */

static int parse_pid_list(pid_t **array, size_t *count, const char *value) {
  char *copy;
  char *token;
  char *saveptr;

  if (value == NULL) {
    return -1;
  }

  copy = strdup(value);

  if (copy == NULL) {
    return -1;
  }

  token = strtok_r(copy, ",", &saveptr);

  while (token != NULL) {

    char *endptr;
    long pid;

    errno = 0;

    pid = strtol(token, &endptr, 10);

    if (errno != 0 || endptr == token || *endptr != '\0' || pid <= 0) {

      free(copy);
      return -1;
    }

    if (append_pid(array, count, (pid_t)pid) != 0) {

      free(copy);
      return -1;
    }

    token = strtok_r(NULL, ",", &saveptr);
  }

  free(copy);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Parse state filter                                                        */
/* ------------------------------------------------------------------------- */

static int parse_states(NeoConfig *config, const char *value) {
  size_t i;

  if (config == NULL || value == NULL) {
    return -1;
  }

  for (i = 0; value[i] != '\0' && config->state_count < MAX_STATE_FILTER; ++i) {

    char state = value[i];

    if (state == ',') {
      continue;
    }

    state = (char)toupper((unsigned char)state);

    /*
     * Linux process states:
     *
     * R - running
     * S - sleeping
     * D - uninterruptible sleep
     * T - stopped
     * t - tracing stop
     * Z - zombie
     * X - dead
     * I - idle
     */
    if (state == 'R' || state == 'S' || state == 'D' || state == 'T' ||
        state == 'Z' || state == 'X' || state == 'I') {

      config->states[config->state_count++] = state;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* User lookup                                                               */
/* ------------------------------------------------------------------------- */

static int parse_user(NeoConfig *config, const char *value) {
  struct passwd *pwd;
  char *endptr;
  unsigned long uid;

  if (config == NULL || value == NULL) {
    return -1;
  }

  /*
   * Numeric UID.
   */
  errno = 0;

  uid = strtoul(value, &endptr, 10);

  if (errno == 0 && endptr != value && *endptr == '\0') {

    config->filter_uid = (uid_t)uid;

    config->filter_uid_enabled = true;

    return 0;
  }

  /*
   * Username.
   */
  pwd = getpwnam(value);

  if (pwd == NULL) {
    return -1;
  }

  config->filter_uid = pwd->pw_uid;

  config->filter_uid_enabled = true;

  snprintf(config->filter_user, sizeof(config->filter_user), "%s", value);

  config->filter_user_enabled = true;

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Usage                                                                     */
/* ------------------------------------------------------------------------- */

void print_usage(const char *program) {
  if (program == NULL) {
    program = "monitoring_services";
  }

  printf("Usage: %s [OPTIONS] [PATTERN ...]\n"
         "\n"
         "Linux process monitor inspired by top.\n"
         "\n"
         "If no PATTERN is supplied, all processes are monitored.\n"
         "\n"
         "Filtering:\n"
         "  PATTERN                  Match command name/cmdline\n"
         "  -x, --exclude PATTERN    Exclude matching processes\n"
         "  -p, --pid PID[,PID...]   Filter by PID\n"
         "  -P, --ppid PID[,PID...]  Filter by parent PID\n"
         "  -u, --user USER|UID      Filter by user or UID\n"
         "  -s, --state STATES       Filter by process state\n"
         "\n"
         "Thresholds:\n"
         "      --cpu-threshold N   Minimum CPU %%\n"
         "      --ram-mb N          Minimum RSS in MB\n"
         "      --ram-percent N     Minimum RAM %%\n"
         "\n"
         "Display:\n"
         "  -l, --long-args         Show full command line\n"
         "      --swap              Show swap usage\n"
         "      --io                Show I/O rates\n"
         "      --threads           Show thread count\n"
         "      --start-time        Show process start time\n"
         "      --elapsed           Show elapsed process time\n"
         "  -n, --limit N           Display at most N processes\n"
         "\n"
         "Sorting:\n"
         "  -c, --sort cpu          Sort by CPU\n"
         "  -m, --sort mem          Sort by memory\n"
         "      --sort pid          Sort by PID\n"
         "      --sort rss          Sort by RSS\n"
         "      --sort read         Sort by read rate\n"
         "      --sort write        Sort by write rate\n"
         "  -R, --reverse           Reverse sort order\n"
         "\n"
         "Execution/output:\n"
         "  -i, --interval SEC      Refresh interval\n"
         "  -1, --once              Display one snapshot and exit\n"
         "  -b, --batch             Non-interactive batch mode\n"
         "      --csv               CSV output\n"
         "      --json              JSON output\n"
         "      --no-color          Disable terminal colors\n"
         "  -h, --help              Show this help\n"
         "  -V, --version           Show version\n"
         "\n"
         "Remote card (SSH/Telnet monitoring, FTP/TFTP deploy):\n"
         "      --protocol NAME     local (default), ssh, telnet, ftp, "
         "tftp\n"
         "      --host HOST         Remote card address\n"
         "  -u, --user USER         Login username (ssh/telnet/ftp)\n"
         "      --password PASS     Login password (telnet/ftp)\n"
         "      --port N            Override the protocol's default port\n"
         "      --identity KEYFILE  SSH private key\n"
         "      --remote-bin NAME   Name of this tool on the card "
         "(ssh/telnet)\n"
         "      --local-file PATH   File to upload (ftp/tftp)\n"
         "      --remote-file NAME  Destination filename (ftp/tftp)\n"
         "\n"
         "  Filtering, sorting and output flags above still apply to "
         "ssh/telnet\n"
         "  results (fetched from the card, then filtered/sorted "
         "locally).\n"
         "\n"
         "Interactive keys:\n"
         "  q  quit\n"
         "  c  sort CPU\n"
         "  m  sort memory\n"
         "  p  sort PID\n"
         "  +  increase refresh interval\n"
         "  -  decrease refresh interval\n"
         "  r  refresh immediately\n"
         "  v  reverse sorting\n"
         "\n",
         program);
}

/* ------------------------------------------------------------------------- */
/* Configuration parser                                                      */
/* ------------------------------------------------------------------------- */

int config_parse(NeoConfig *config, int argc, char **argv) {
  int option;
  int option_index = 0;

  static const struct option long_options[] = {
      {"interval", required_argument, 0, 'i'},
      {"long-args", no_argument, 0, 'l'},
      {"exclude", required_argument, 0, 'x'},
      {"pid", required_argument, 0, 'p'},
      {"ppid", required_argument, 0, 'P'},
      {"user", required_argument, 0, 'u'},
      {"state", required_argument, 0, 's'},

      {"cpu-threshold", required_argument, 0, 1000},
      {"ram-mb", required_argument, 0, 1001},
      {"ram-percent", required_argument, 0, 1002},

      {"swap", no_argument, 0, 1003},
      {"io", no_argument, 0, 1004},
      {"threads", no_argument, 0, 1005},
      {"start-time", no_argument, 0, 1006},
      {"elapsed", no_argument, 0, 1007},

      {"sort", required_argument, 0, 1008},
      {"reverse", no_argument, 0, 'R'},

      {"limit", required_argument, 0, 'n'},

      {"once", no_argument, 0, '1'},
      {"batch", no_argument, 0, 'b'},

      {"csv", no_argument, 0, 1009},
      {"json", no_argument, 0, 1010},
      {"no-color", no_argument, 0, 1011},

      {"protocol", required_argument, 0, 2000},
      {"host", required_argument, 0, 2001},
      {"port", required_argument, 0, 2002},
      {"identity", required_argument, 0, 2003},
      {"password", required_argument, 0, 2004},
      {"remote-bin", required_argument, 0, 2005},
      {"local-file", required_argument, 0, 2006},
      {"remote-file", required_argument, 0, 2007},

      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'V'},

      {0, 0, 0, 0}};

  const char *user_arg = NULL;

  if (config == NULL) {
    return -1;
  }

  opterr = 0;

  while ((option = getopt_long(argc, argv, "i:lx:p:P:u:s:n:1bRhVcm",
                               long_options, &option_index)) != -1) {

    switch (option) {

    case 'i':
      config->interval = strtod(optarg, NULL);

      if (config->interval < MIN_INTERVAL || config->interval > MAX_INTERVAL) {

        fprintf(stderr, "Invalid interval: %s\n", optarg);

        return -1;
      }
      break;

    case 'l':
      config->show_long_args = true;
      break;

    case 'x':
      if (config->exclude_count >= MAX_PATTERNS) {

        fprintf(stderr, "Too many exclude patterns.\n");

        return -1;
      }

      if (append_pattern(&config->exclude_patterns, &config->exclude_count,
                         optarg) != 0) {
        return -1;
      }
      break;

    case 'p':
      if (parse_pid_list(&config->pids, &config->pid_count, optarg) != 0) {

        fprintf(stderr, "Invalid PID list: %s\n", optarg);

        return -1;
      }
      break;

    case 'P':
      if (parse_pid_list(&config->ppids, &config->ppid_count, optarg) != 0) {

        fprintf(stderr, "Invalid PPID list: %s\n", optarg);

        return -1;
      }
      break;

    case 'u':
      /*
       * `--user` is dual-purpose: in local mode it is validated below
       * as a local process filter (existing behavior, preserved
       * exactly). In a remote mode (--protocol ssh/telnet/ftp) it is
       * simply the login username on the remote host and may not
       * exist as a local account at all, so validation is skipped
       * for those modes once the final protocol is known.
       */
      user_arg = optarg;

      snprintf(config->remote_user, sizeof(config->remote_user), "%s", optarg);
      break;

    case 's':
      if (parse_states(config, optarg) != 0) {
        return -1;
      }
      break;

    case 'n':
      config->limit = strtoul(optarg, NULL, 10);
      break;

    case '1':
      config->once = true;
      break;

    case 'b':
      config->batch = true;
      break;

    case 'R':
      config->reverse = true;
      break;

    case 'c':
      config->sort_mode = SORT_CPU;
      break;

    case 'm':
      config->sort_mode = SORT_MEM;
      break;

    case 1000:
      config->cpu_threshold = strtod(optarg, NULL);
      break;

    case 1001:
      config->ram_threshold_mb = strtod(optarg, NULL);
      break;

    case 1002:
      config->ram_threshold_percent = strtod(optarg, NULL);
      break;

    case 1003:
      config->show_swap = true;
      break;

    case 1004:
      config->show_io = true;
      break;

    case 1005:
      config->show_threads = true;
      break;

    case 1006:
      config->show_start_time = true;
      break;

    case 1007:
      config->show_elapsed = true;
      break;

    case 1008:

      if (strcasecmp(optarg, "cpu") == 0) {
        config->sort_mode = SORT_CPU;

      } else if (strcasecmp(optarg, "mem") == 0 ||
                 strcasecmp(optarg, "memory") == 0) {
        config->sort_mode = SORT_MEM;

      } else if (strcasecmp(optarg, "pid") == 0) {
        config->sort_mode = SORT_PID;

      } else if (strcasecmp(optarg, "rss") == 0) {
        config->sort_mode = SORT_RSS;

      } else if (strcasecmp(optarg, "read") == 0) {
        config->sort_mode = SORT_IO_READ;

      } else if (strcasecmp(optarg, "write") == 0) {
        config->sort_mode = SORT_IO_WRITE;

      } else {
        fprintf(stderr, "Unknown sort mode: %s\n", optarg);

        return -1;
      }

      break;

    case 1009:
      config->csv = true;
      config->batch = true;
      break;

    case 1010:
      config->json = true;
      config->batch = true;
      break;

    case 1011:
      config->no_color = true;
      break;

    case 2000:

      if (strcasecmp(optarg, "local") == 0) {
        config->protocol = PROTOCOL_LOCAL;

      } else if (strcasecmp(optarg, "ssh") == 0) {
        config->protocol = PROTOCOL_SSH;

      } else if (strcasecmp(optarg, "telnet") == 0) {
        config->protocol = PROTOCOL_TELNET;

      } else if (strcasecmp(optarg, "ftp") == 0) {
        config->protocol = PROTOCOL_FTP;

      } else if (strcasecmp(optarg, "tftp") == 0) {
        config->protocol = PROTOCOL_TFTP;

      } else {
        fprintf(stderr,
                "Unknown protocol: %s (expected local, ssh, telnet, "
                "ftp or tftp)\n",
                optarg);

        return -1;
      }

      break;

    case 2001:
      snprintf(config->remote_host, sizeof(config->remote_host), "%s", optarg);
      break;

    case 2002:
      config->remote_port = atoi(optarg);
      break;

    case 2003:
      snprintf(config->remote_identity, sizeof(config->remote_identity), "%s",
               optarg);
      break;

    case 2004:
      snprintf(config->remote_password, sizeof(config->remote_password), "%s",
               optarg);
      break;

    case 2005:
      snprintf(config->remote_binary, sizeof(config->remote_binary), "%s",
               optarg);
      break;

    case 2006:
      snprintf(config->local_file, sizeof(config->local_file), "%s", optarg);
      break;

    case 2007:
      snprintf(config->remote_file, sizeof(config->remote_file), "%s", optarg);
      break;

    case 'h':
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);

    case 'V':
      printf("monitoring_services %s\n", VERSION);
      exit(EXIT_SUCCESS);

    case '?':
    default:
      fprintf(stderr, "Unknown or incomplete option. "
                      "Use --help.\n");

      return -1;
    }
  }

  /*
   * Remaining positional arguments are include patterns.
   */
  while (optind < argc) {

    if (config->pattern_count >= MAX_PATTERNS) {

      fprintf(stderr, "Too many patterns.\n");

      return -1;
    }

    if (append_pattern(&config->patterns, &config->pattern_count,
                       argv[optind]) != 0) {

      return -1;
    }

    ++optind;
  }

  /*
   * JSON/CSV are inherently non-interactive.
   */
  if (config->csv || config->json) {
    config->batch = true;
  }

  /*
   * Only validate --user as a local account/UID when it is actually
   * going to be used as a local process filter. In a remote mode it
   * is a login username on the remote host instead.
   */
  if (config->protocol == PROTOCOL_LOCAL && user_arg != NULL) {

    if (parse_user(config, user_arg) != 0) {

      fprintf(stderr, "Unknown user or invalid UID: %s\n", user_arg);

      return -1;
    }
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Process list initialization                                               */
/* ------------------------------------------------------------------------- */

void process_list_init(NeoProcessList *list) {
  if (list == NULL) {
    return;
  }

  memset(list, 0, sizeof(*list));
}

void process_list_free(NeoProcessList *list) {
  if (list == NULL) {
    return;
  }

  free(list->items);

  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

/* ------------------------------------------------------------------------- */
/* Previous list initialization                                              */
/* ------------------------------------------------------------------------- */

void previous_list_init(NeoPreviousList *list) {
  if (list == NULL) {
    return;
  }

  memset(list, 0, sizeof(*list));
}

void previous_list_free(NeoPreviousList *list) {
  if (list == NULL) {
    return;
  }

  free(list->items);

  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

/* ------------------------------------------------------------------------- */
/* System information                                                        */
/* ------------------------------------------------------------------------- */

int read_system_info(NeoSystemInfo *info) {
  long cpu_count;

  if (info == NULL) {
    return -1;
  }

  memset(info, 0, sizeof(*info));

  info->total_cpu = read_system_cpu();

  if (read_total_memory(&info->total_memory_kb) != 0) {
    return -1;
  }

  cpu_count = sysconf(_SC_NPROCESSORS_ONLN);

  if (cpu_count < 1) {
    cpu_count = 1;
  }

  info->cpu_count = (unsigned int)cpu_count;

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Process scanning                                                          */
/* ------------------------------------------------------------------------- */

int scan_processes(const NeoConfig *config, const NeoSystemInfo *system,
                   NeoProcessList *list, NeoPreviousList *previous,
                   double interval) {
  DIR *dir;
  struct dirent *entry;

  if (config == NULL || system == NULL || list == NULL || previous == NULL) {
    return -1;
  }

  process_list_free(list);
  process_list_init(list);

  dir = opendir("/proc");

  if (dir == NULL) {
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {

    pid_t pid;
    NeoProcess process;

    int previous_index;
    const NeoPreviousSample *previous_sample;

    /*
     * Only numeric /proc entries represent processes.
     */
    if (parse_pid_path(entry->d_name, &pid) != 0) {
      continue;
    }

    if (process_collect(pid, &process) != 0) {
      continue;
    }

    /*
     * Apply structural filters before expensive metrics.
     */
    if (!process_matches(config, &process)) {
      continue;
    }

    /*
     * Find the previous sample.
     */
    previous_index = previous_find(previous, process.pid);

    if (previous_index >= 0) {
      previous_sample = &previous->items[previous_index];
    } else {
      previous_sample = NULL;
    }

    /*
     * Calculate metrics.
     */
    update_metrics(&process, previous_sample, system, interval);

    /*
     * Threshold filtering.
     */
    if (config->cpu_threshold > 0.0 &&
        process.cpu_percent < config->cpu_threshold) {
      /*
       * Still update the baseline below.
       */
    } else if (config->ram_threshold_mb > 0.0 &&
               process.rss_mb < config->ram_threshold_mb) {
      /*
       * Still update the baseline below.
       */
    } else if (config->ram_threshold_percent > 0.0 &&
               process.mem_percent < config->ram_threshold_percent) {
      /*
       * Still update the baseline below.
       */
    } else {

      if (process_list_add(list, &process) != 0) {

        closedir(dir);
        return -1;
      }
    }

    /*
     * Update baseline even when the process is rejected by a
     * threshold. This prevents a large artificial CPU/I/O delta
     * when a process crosses the threshold later.
     */
    if (previous_set(previous, &process) != 0) {

      closedir(dir);
      return -1;
    }
  }

  closedir(dir);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Sorting                                                                   */
/* ------------------------------------------------------------------------- */

static int compare_processes(const NeoProcess *a, const NeoProcess *b,
                             NeoSortMode mode) {
  switch (mode) {

  case SORT_CPU:
    if (a->cpu_percent < b->cpu_percent) {
      return 1;
    }

    if (a->cpu_percent > b->cpu_percent) {
      return -1;
    }

    break;

  case SORT_MEM:
    if (a->mem_percent < b->mem_percent) {
      return 1;
    }

    if (a->mem_percent > b->mem_percent) {
      return -1;
    }

    break;

  case SORT_PID:
    if (a->pid > b->pid) {
      return 1;
    }

    if (a->pid < b->pid) {
      return -1;
    }

    break;

  case SORT_RSS:
    if (a->rss_kb < b->rss_kb) {
      return 1;
    }

    if (a->rss_kb > b->rss_kb) {
      return -1;
    }

    break;

  case SORT_IO_READ:
    if (a->io_read_mb_s < b->io_read_mb_s) {
      return 1;
    }

    if (a->io_read_mb_s > b->io_read_mb_s) {
      return -1;
    }

    break;

  case SORT_IO_WRITE:
    if (a->io_write_mb_s < b->io_write_mb_s) {
      return 1;
    }

    if (a->io_write_mb_s > b->io_write_mb_s) {
      return -1;
    }

    break;

  default:
    break;
  }

  /*
   * Stable deterministic tie-breaker.
   */
  if (a->pid > b->pid) {
    return 1;
  }

  if (a->pid < b->pid) {
    return -1;
  }

  return 0;
}

static NeoSortMode sort_mode_global;

static int qsort_compare(const void *left, const void *right) {
  const NeoProcess *a = (const NeoProcess *)left;

  const NeoProcess *b = (const NeoProcess *)right;

  return compare_processes(a, b, sort_mode_global);
}

void sort_processes(NeoProcessList *list, NeoSortMode mode, bool reverse) {
  size_t i;
  size_t j;

  if (list == NULL || list->items == NULL || list->count < 2) {
    return;
  }

  /*
   * qsort is used for the main ordering.
   */
  sort_mode_global = mode;

  qsort(list->items, list->count, sizeof(list->items[0]), qsort_compare);

  /*
   * Reverse the resulting order if requested.
   */
  if (reverse) {

    for (i = 0, j = list->count - 1; i < j; ++i, --j) {

      NeoProcess tmp = list->items[i];

      list->items[i] = list->items[j];

      list->items[j] = tmp;
    }
  }
}
