#define _GNU_SOURCE

#include "monitoring_output.h"
#include "monitoring_services.h"
#include "monitoring_ui.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv) {
  NeoConfig config;
  NeoSystemInfo system_info;

  NeoProcessList process_list;
  NeoPreviousList previous_list;

  int interactive = 0;
  int ui_initialized = 0;

  double interval;

  config_init(&config);
  process_list_init(&process_list);
  previous_list_init(&previous_list);

  /* ------------------------------------------------------------------ */
  /* Parse command line                                                  */
  /* ------------------------------------------------------------------ */

  if (config_parse(&config, argc, argv) != 0) {

    config_free(&config);
    return EXIT_FAILURE;
  }

  /* ------------------------------------------------------------------ */
  /* Install signal handlers                                             */
  /* ------------------------------------------------------------------ */

  if (install_signals() != 0) {

    fprintf(stderr, "monitoring_services: "
                    "failed to install signal handlers\n");

    config_free(&config);
    return EXIT_FAILURE;
  }

  /* ------------------------------------------------------------------ */
  /* Determine terminal mode                                             */
  /* ------------------------------------------------------------------ */

  interactive =
      ui_is_interactive() && !config.batch && !config.csv && !config.json;

  if (interactive) {

    if (ui_setup() != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to initialize terminal\n");

      config_free(&config);
      return EXIT_FAILURE;
    }

    ui_initialized = 1;

    ui_hide_cursor();
  }

  /* ------------------------------------------------------------------ */
  /* Main monitoring loop                                                */
  /* ------------------------------------------------------------------ */

  interval = config.interval;

  while (running) {

    int force_refresh = 0;

    /*
     * Read current system information.
     */
    if (read_system_info(&system_info) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to read system information\n");

      break;
    }

    /*
     * Scan /proc and calculate process metrics.
     */
    if (scan_processes(&config, &system_info, &process_list, &previous_list,
                           interval) != 0) {

      fprintf(stderr, "monitoring_services: "
                      "failed to scan /proc\n");

      break;
    }

    /*
     * Sort before output.
     */
    sort_processes(&process_list, config.sort_mode, config.reverse);

    /* -------------------------------------------------------------- */
    /* Output                                                         */
    /* -------------------------------------------------------------- */

    if (config.json) {

      output_json(&config, &system_info, &process_list);

    } else if (config.csv) {

      output_csv(&config, &process_list);

    } else if (interactive) {

      ui_clear();

      output_table(&config, &system_info, &process_list);

    } else {

      /*
       * Batch mode without CSV/JSON uses the normal table.
       */
      output_table(&config, &system_info, &process_list);
    }

    /* -------------------------------------------------------------- */
    /* One-shot mode                                                   */
    /* -------------------------------------------------------------- */

    if (config.once) {
      break;
    }

    /*
     * CSV and JSON are intended primarily for snapshots.
     * Batch mode therefore performs one scan unless the caller
     * explicitly uses normal interactive operation.
     */
    if (config.batch && !interactive) {
      break;
    }

    /* -------------------------------------------------------------- */
    /* Interactive wait                                                */
    /* -------------------------------------------------------------- */

    if (interactive) {

      int key;

      key = ui_wait(config.interval);

      if (key >= 0) {

        if (!ui_handle_key(&config, key, &force_refresh)) {

          running = 0;
          break;
        }

        if (force_refresh) {
          continue;
        }
      }

    } else {

      /*
       * Non-interactive fallback.
       */
      sleep((unsigned int)(config.interval > 1.0 ? config.interval : 1.0));
    }

    /*
     * Preserve the current interactive interval.
     */
    interval = config.interval;
  }

  /* ------------------------------------------------------------------ */
  /* Cleanup                                                             */
  /* ------------------------------------------------------------------ */

  if (ui_initialized) {

    ui_restore();

    /*
     * Move to a clean line after the live display.
     */
    putchar('\n');
  }

  process_list_free(&process_list);

  previous_list_free(&previous_list);

  config_free(&config);

  return running ? EXIT_SUCCESS : EXIT_SUCCESS;
}
