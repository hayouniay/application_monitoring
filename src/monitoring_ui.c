#define _GNU_SOURCE

#include "monitoring_ui.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */
/* Internal terminal state                                                   */
/* ------------------------------------------------------------------------- */

static struct termios original_terminal;
static int terminal_configured = 0;

/* ------------------------------------------------------------------------- */
/* Terminal setup                                                            */
/* ------------------------------------------------------------------------- */

int ui_setup(void) {
  struct termios raw;

  if (!ui_is_interactive()) {
    return 0;
  }

  if (terminal_configured) {
    return 0;
  }

  if (tcgetattr(STDIN_FILENO, &original_terminal) != 0) {
    return -1;
  }

  raw = original_terminal;

  /*
   * Disable canonical mode:
   * input becomes available immediately, without Enter.
   */
  raw.c_lflag &= ~(ICANON | ECHO);

  /*
   * Do not wait for characters inside read().
   * We use select() for timing.
   */
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  /*
   * Keep terminal output processing enabled.
   */
  raw.c_oflag |= OPOST;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    return -1;
  }

  terminal_configured = 1;

  return 0;
}

/* ------------------------------------------------------------------------- */
/* Terminal restore                                                          */
/* ------------------------------------------------------------------------- */

void ui_restore(void) {
  if (!terminal_configured) {
    return;
  }

  (void)tcsetattr(STDIN_FILENO, TCSANOW, &original_terminal);

  terminal_configured = 0;

  /*
   * Make sure the cursor is visible again.
   */
  ui_show_cursor();

  fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* Cursor                                                                     */
/* ------------------------------------------------------------------------- */

void ui_hide_cursor(void) {
  if (!ui_is_interactive()) {
    return;
  }

  fputs("\033[?25l", stdout);
  fflush(stdout);
}

void ui_show_cursor(void) {
  if (!ui_is_interactive()) {
    return;
  }

  fputs("\033[?25h", stdout);
  fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* Screen                                                                     */
/* ------------------------------------------------------------------------- */

void ui_clear(void) {
  if (!ui_is_interactive()) {
    return;
  }

  /*
   * Clear screen and move cursor to top-left.
   */
  fputs("\033[2J\033[H", stdout);

  fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* Interactive terminal detection                                            */
/* ------------------------------------------------------------------------- */

int ui_is_interactive(void) {
  return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
}

/* ------------------------------------------------------------------------- */
/* Read one key                                                              */
/* ------------------------------------------------------------------------- */

int ui_read_key(void) {
  unsigned char key;
  ssize_t result;

  if (!ui_is_interactive()) {
    return -1;
  }

  result = read(STDIN_FILENO, &key, sizeof(key));

  if (result == 1) {
    return (int)key;
  }

  return -1;
}

/* ------------------------------------------------------------------------- */
/* Wait while remaining responsive                                           */
/* ------------------------------------------------------------------------- */

int ui_wait(double seconds) {
  struct timeval timeout;
  fd_set readfds;

  int result;

  if (seconds < 0.0) {
    seconds = 0.0;
  }

  timeout.tv_sec = (time_t)seconds;

  timeout.tv_usec =
      (suseconds_t)((seconds - (double)timeout.tv_sec) * 1000000.0);

  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

  if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {

    return ui_read_key();
  }

  if (result == 0) {
    return -1;
  }

  /*
   * Interrupted system call is not an error for our use case.
   */
  if (result < 0 && errno == EINTR) {
    return -1;
  }

  return -1;
}

/* ------------------------------------------------------------------------- */
/* Interactive command handling                                              */
/* ------------------------------------------------------------------------- */

int ui_handle_key(NeoConfig *config, int key, int *force_refresh) {
  if (config == NULL) {
    return 0;
  }

  if (force_refresh != NULL) {
    *force_refresh = 0;
  }

  switch (key) {

    /* ------------------------------------------------------------- */
    /* Quit                                                          */
    /* ------------------------------------------------------------- */

  case 'q':
  case 'Q':
    return 0;

    /* ------------------------------------------------------------- */
    /* CPU sort                                                      */
    /* ------------------------------------------------------------- */

  case 'c':
  case 'C':
    config->sort_mode = NEO_SORT_CPU;

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Memory sort                                                   */
    /* ------------------------------------------------------------- */

  case 'm':
  case 'M':
    config->sort_mode = NEO_SORT_MEM;

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* PID sort                                                       */
    /* ------------------------------------------------------------- */

  case 'p':
  case 'P':
    config->sort_mode = NEO_SORT_PID;

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Increase refresh interval                                     */
    /* ------------------------------------------------------------- */

  case '+':
    config->interval *= 2.0;

    if (config->interval > NEO_MAX_INTERVAL) {
      config->interval = NEO_MAX_INTERVAL;
    }

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Decrease refresh interval                                     */
    /* ------------------------------------------------------------- */

  case '-':
    config->interval /= 2.0;

    if (config->interval < NEO_MIN_INTERVAL) {
      config->interval = NEO_MIN_INTERVAL;
    }

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Force refresh                                                  */
    /* ------------------------------------------------------------- */

  case 'r':
  case 'R':

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Reverse sorting                                                */
    /* ------------------------------------------------------------- */

  case 'v':
  case 'V':
    config->reverse = !config->reverse;

    if (force_refresh != NULL) {
      *force_refresh = 1;
    }

    break;

    /* ------------------------------------------------------------- */
    /* Unknown key                                                    */
    /* ------------------------------------------------------------- */

  default:
    break;
  }

  return 1;
}
