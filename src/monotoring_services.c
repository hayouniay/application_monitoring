#define _GNU_SOURCE

#include "monitoring_alert.h"
#include "monitoring_capture.h"
#include "monitoring_log.h"
#include "monitoring_output.h"
#include "monitoring_remote.h"
#include "monitoring_services.h"
#include "monitoring_ui.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Global termination flag                                                   */
/* ------------------------------------------------------------------------- */

static volatile sig_atomic_t running = 1;

/* ------------------------------------------------------------------------- */
/* Signal handler                                                            */
/* ------------------------------------------------------------------------- */

static void signal_handler(int signal_number) {
  (void)signal_number;

  running = 0;
}

/* ------------------------------------------------------------------------- */
/* Install signal handlers                                                   */
/* ------------------------------------------------------------------------- */

static int install_signals(void) {
  struct sigaction action;

  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGINT, &action, NULL) != 0) {
    return -1;
  }

  if (sigaction(SIGTERM, &action, NULL) != 0) {
    return -1;
  }

  /*
   * Prevent accidental termination when stdout is closed.
   */
  signal(SIGPIPE, SIG_IGN);

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Remote target construction                                                */
/* ------------------------------------------------------------------------- */

static void build_remote_target(const NeoConfig *config,
                                NeoRemoteTarget *target) {
  remote_target_init(target);

  snprintf(target->host, sizeof(target->host), "%s", config->remote_host);
  snprintf(target->user, sizeof(target->user), "%s", config->remote_user);
  snprintf(target->password, sizeof(target->password), "%s",
           config->remote_password);
  snprintf(target->identity_file, sizeof(target->identity_file), "%s",
           config->remote_identity);
  snprintf(target->remote_binary, sizeof(target->remote_binary), "%s",
           config->remote_binary[0] ? config->remote_binary
                                    : "app_top_monitoring");

  target->port = config->remote_port;
}

static void build_ftp_target(const NeoConfig *config, NeoFtpTarget *target) {
  ftp_target_init(target);

  snprintf(target->host, sizeof(target->host), "%s", config->remote_host);
  snprintf(target->user, sizeof(target->user), "%s", config->remote_user);
  snprintf(target->password, sizeof(target->password), "%s",
           config->remote_password);
  snprintf(target->remote_filename, sizeof(target->remote_filename), "%s",
           config->remote_file);

  if (config->remote_port > 0) {
    target->port = config->remote_port;
  }
}

static void build_tftp_target(const NeoConfig *config, NeoTftpTarget *target) {
  tftp_target_init(target);

  snprintf(target->host, sizeof(target->host), "%s", config->remote_host);
  snprintf(target->remote_filename, sizeof(target->remote_filename), "%s",
           config->remote_file);

  if (config->remote_port > 0) {
    target->port = config->remote_port;
  }
}

/* ------------------------------------------------------------------------- */
/* Local filters applied client-side to remotely fetched processes           */
/* ------------------------------------------------------------------------- */

static void apply_local_filters(const NeoConfig *config, NeoProcessList *list) {
  size_t read_index;
  size_t write_index = 0;

  for (read_index = 0; read_index < list->count; ++read_index) {

    if (process_matches(config, &list->items[read_index])) {

      if (write_index != read_index) {
        list->items[write_index] = list->items[read_index];
      }

      ++write_index;
    }
  }

  list->count = write_index;
}

/* ------------------------------------------------------------------------- */
/* Remote monitoring (ssh / telnet)                                          */
/* ------------------------------------------------------------------------- */

static int run_remote_monitor(NeoConfig *config) {
  NeoRemoteTarget target;
  NeoProcessList list;
  NeoSystemInfo system_info;

  char message[REMOTE_MESSAGE_MAX];

  if (config->remote_host[0] == '\0') {
    fprintf(stderr, "monitoring_services: --host is required for "
                    "--protocol ssh/telnet\n");
    return EXIT_FAILURE;
  }

  build_remote_target(config, &target);

  memset(&system_info, 0, sizeof(system_info));

  process_list_init(&list);

  log_write(NEO_LOG_INFO, "Starting remote monitoring via %s: %s%s%s",
           config->protocol == PROTOCOL_SSH ? "SSH" : "Telnet",
           target.user[0] ? target.user : "", target.user[0] ? "@" : "",
           target.host);

  while (running) {

    int result;

    if (config->protocol == PROTOCOL_SSH) {
      result = remote_ssh_scan(&target, &list, &system_info, message,
                               sizeof(message));
    } else {
      result = remote_telnet_scan(&target, &list, &system_info, message,
                                  sizeof(message));
    }

    if (result != 0) {

      fprintf(stderr, "monitoring_services: %s\n", message);

      log_write(NEO_LOG_ERROR, "Remote monitoring connection failed: %s",
               message);

      process_list_free(&list);

      return EXIT_FAILURE;
    }

    apply_local_filters(config, &list);

    sort_processes(&list, config->sort_mode, config->reverse);

    if (config->json) {

      output_json(config, &system_info, &list);

    } else if (config->csv) {

      output_csv(config, &list);

    } else {

      printf("Remote monitoring via %s: %s%s%s\n\n",
             config->protocol == PROTOCOL_SSH ? "SSH" : "Telnet",
             target.user[0] ? target.user : "", target.user[0] ? "@" : "",
             target.host);

      output_table(config, &system_info, &list);
    }

    if (config->once) {
      break;
    }

    sleep((unsigned int)(config->interval > 1.0 ? config->interval : 1.0));
  }

  process_list_free(&list);

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Deploy (ftp / tftp)                                                       */
/* ------------------------------------------------------------------------- */

static int run_deploy(NeoConfig *config) {
  char message[REMOTE_MESSAGE_MAX];

  if (config->remote_host[0] == '\0') {
    fprintf(stderr, "monitoring_services: --host is required for "
                    "--protocol ftp/tftp\n");
    return EXIT_FAILURE;
  }

  if (config->local_file[0] == '\0') {
    fprintf(stderr, "monitoring_services: --local-file is required for "
                    "--protocol ftp/tftp\n");
    return EXIT_FAILURE;
  }

  printf("Deploying %s to %s via %s...\n", config->local_file,
         config->remote_host,
         config->protocol == PROTOCOL_FTP ? "FTP" : "TFTP");

  log_write(NEO_LOG_INFO, "Deploying %s to %s via %s", config->local_file,
           config->remote_host,
           config->protocol == PROTOCOL_FTP ? "FTP" : "TFTP");

  if (config->protocol == PROTOCOL_FTP) {

    NeoFtpTarget target;

    build_ftp_target(config, &target);

    if (remote_ftp_deploy(&target, config->local_file, message,
                          sizeof(message)) != 0) {

      fprintf(stderr, "monitoring_services: %s\n", message);

      log_write(NEO_LOG_ERROR, "FTP deploy failed: %s", message);

      return EXIT_FAILURE;
    }

  } else {

    NeoTftpTarget target;

    build_tftp_target(config, &target);

    if (remote_tftp_deploy(&target, config->local_file, message,
                           sizeof(message)) != 0) {

      fprintf(stderr, "monitoring_services: %s\n", message);

      log_write(NEO_LOG_ERROR, "TFTP deploy failed: %s", message);

      return EXIT_FAILURE;
    }
  }

  printf("Deploy finished successfully.\n");

  log_write(NEO_LOG_INFO, "Deploy finished successfully");

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Local monitoring (original behavior, unchanged)                           */
/* ------------------------------------------------------------------------- */

static int run_local_monitor(NeoConfig *config) {
  NeoSystemInfo system_info;

  NeoProcessList process_list;
  NeoPreviousList previous_list;

  NeoAlertState alert_state;
  bool alert_ever_fired = false;
  const bool alerting_enabled =
      config->alert_cpu_percent > 0.0 || config->alert_mem_percent > 0.0;

  int interactive = 0;
  int ui_initialized = 0;

  double interval;

  process_list_init(&process_list);
  previous_list_init(&previous_list);

  alert_state_init(&alert_state);

  interactive =
      ui_is_interactive() && !config->batch && !config->csv && !config->json;

  if (interactive) {

    if (ui_setup() != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to initialize terminal\n");

      return EXIT_FAILURE;
    }

    ui_initialized = 1;

    ui_hide_cursor();
  }

  interval = config->interval;

  while (running) {

    int force_refresh = 0;

    if (read_system_info(&system_info) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to read system information\n");

      log_write(NEO_LOG_ERROR, "Refresh failed: unable to read system "
                              "information");

      break;
    }

    if (scan_processes(config, &system_info, &process_list, &previous_list,
                       interval) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to scan /proc\n");

      log_write(NEO_LOG_ERROR, "Refresh failed: unable to scan /proc");

      break;
    }

    sort_processes(&process_list, config->sort_mode, config->reverse);

    if (alerting_enabled) {

      double cpu_percent = 0.0;
      double mem_percent = 0.0;
      double swap_percent = 0.0;

      capture_read_system(&cpu_percent, &mem_percent, &swap_percent);

      const int fired =
          alert_evaluate(config, &alert_state, cpu_percent, mem_percent);

      if (fired != 0) {
        alert_ever_fired = true;
        alert_dispatch(config, fired, cpu_percent, mem_percent);
      }
    }

    if (config->json) {

      output_json(config, &system_info, &process_list);

    } else if (config->csv) {

      output_csv(config, &process_list);

    } else if (interactive) {

      ui_clear();

      output_table(config, &system_info, &process_list);

    } else {

      output_table(config, &system_info, &process_list);
    }

    if (config->once) {

      const int exit_code = alert_ever_fired
                                ? NEO_EXIT_ALERT
                                : EXIT_SUCCESS;

      process_list_free(&process_list);
      previous_list_free(&previous_list);

      if (ui_initialized) {
        ui_restore();
        putchar('\n');
      }

      return exit_code;
    }

    if (config->batch && !interactive) {

      const int exit_code = alert_ever_fired
                                ? NEO_EXIT_ALERT
                                : EXIT_SUCCESS;

      process_list_free(&process_list);
      previous_list_free(&previous_list);

      return exit_code;
    }

    if (interactive) {

      int key;

      key = ui_wait(config->interval);

      if (key >= 0) {

        if (!ui_handle_key(config, key, &force_refresh)) {

          running = 0;
          break;
        }

        if (force_refresh) {
          continue;
        }
      }

    } else {

      sleep((unsigned int)(config->interval > 1.0 ? config->interval : 1.0));
    }

    interval = config->interval;
  }

  if (ui_initialized) {

    ui_restore();

    putchar('\n');
  }

  process_list_free(&process_list);

  previous_list_free(&previous_list);

  return alert_ever_fired ? NEO_EXIT_ALERT : EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Capture mode (--capture)                                                  */
/* ------------------------------------------------------------------------- */

static int run_capture_mode(NeoConfig *config) {
  NeoSystemInfo system_info;
  NeoProcessList process_list;
  NeoPreviousList previous_list;
  NeoCaptureSeries series;

  time_t capture_start;
  char csv_path[600];
  char html_path[600];

  process_list_init(&process_list);
  previous_list_init(&previous_list);
  capture_series_init(&series);

  capture_start = time(NULL);

  if (config->capture_duration_seconds > 0.0) {
    printf("Capturing for %.0f second(s)...\n",
           config->capture_duration_seconds);

    log_write(NEO_LOG_INFO, "Capture started (duration=%.0fs)",
             config->capture_duration_seconds);
  } else {
    printf("Capturing... press Ctrl+C to stop.\n");

    log_write(NEO_LOG_INFO, "Capture started (until Ctrl+C)");
  }

  while (running) {

    if (read_system_info(&system_info) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to read system information\n");

      log_write(NEO_LOG_ERROR,
               "Capture failed: unable to read system information");
      break;
    }

    if (scan_processes(config, &system_info, &process_list, &previous_list,
                       config->interval) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to scan /proc\n");

      log_write(NEO_LOG_ERROR, "Capture failed: unable to scan /proc");
      break;
    }

    sort_processes(&process_list, config->sort_mode, config->reverse);

    if (capture_sample(&series, &process_list) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to record capture sample\n");
      break;
    }

    printf("\rCaptured %zu sample(s) (%.0fs elapsed)...", series.count,
           difftime(time(NULL), capture_start));

    fflush(stdout);

    if (config->capture_duration_seconds > 0.0 &&
        difftime(time(NULL), capture_start) >=
            config->capture_duration_seconds) {
      break;
    }

    sleep((unsigned int)(config->interval > 1.0 ? config->interval : 1.0));
  }

  putchar('\n');

  log_write(NEO_LOG_INFO, "Capture stopped: %zu sample(s) captured",
           series.count);

  if (series.count == 0) {

    fprintf(stderr, "monitoring_services: no samples were captured\n");

    process_list_free(&process_list);
    previous_list_free(&previous_list);
    capture_series_free(&series);

    return EXIT_FAILURE;
  }

  if (config->capture_output[0] != '\0') {

    snprintf(csv_path, sizeof(csv_path), "%s.csv", config->capture_output);
    snprintf(html_path, sizeof(html_path), "%s.html", config->capture_output);

  } else {

    const time_t now = time(NULL);

    snprintf(csv_path, sizeof(csv_path), "capture_%lld.csv", (long long)now);
    snprintf(html_path, sizeof(html_path), "capture_%lld.html", (long long)now);
  }

  if (capture_write_csv(&series, csv_path) != 0) {
    fprintf(stderr, "monitoring_services: failed to write %s\n", csv_path);
  } else {
    printf("Wrote %s\n", csv_path);
  }

  if (capture_write_html_report(&series, html_path) != 0) {
    fprintf(stderr, "monitoring_services: failed to write %s\n", html_path);
  } else {
    printf("Wrote %s - open it in a browser to view the graphs.\n", html_path);
  }

  process_list_free(&process_list);
  previous_list_free(&previous_list);
  capture_series_free(&series);

  return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv) {
  NeoConfig config;
  int result;

  config_init(&config);

  if (config_parse(&config, argc, argv) != 0) {

    config_free(&config);
    return EXIT_FAILURE;
  }

  if (log_init(config.log_file[0] ? config.log_file : NULL,
              (NeoLogLevel)config.log_level) != 0) {

    fprintf(stderr,
            "monitoring_services: failed to open log file '%s': %s\n",
            config.log_file, strerror(errno));

    config_free(&config);
    return EXIT_FAILURE;
  }

  if (install_signals() != 0) {

    fprintf(stderr, "monitoring_services: "
                    "failed to install signal handlers\n");

    log_shutdown();
    config_free(&config);
    return EXIT_FAILURE;
  }

  if (config.capture_enabled && config.protocol != PROTOCOL_LOCAL) {

    fprintf(stderr, "monitoring_services: --capture currently only "
                    "supports local monitoring (not with --protocol)\n");

    log_shutdown();
    config_free(&config);
    return EXIT_FAILURE;
  }

  switch (config.protocol) {

  case PROTOCOL_SSH:
  case PROTOCOL_TELNET:
    result = run_remote_monitor(&config);
    break;

  case PROTOCOL_FTP:
  case PROTOCOL_TFTP:
    result = run_deploy(&config);
    break;

  case PROTOCOL_LOCAL:
  default:
    result = config.capture_enabled ? run_capture_mode(&config)
                                    : run_local_monitor(&config);
    break;
  }

  log_shutdown();
  config_free(&config);

  return result;
}
