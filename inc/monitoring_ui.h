#ifndef NEO_MONITORING_UI_H
#define NEO_MONITORING_UI_H

#include "monitoring_services.h"

/*
 * Initialize interactive terminal mode.
 *
 * Returns:
 *   0  success
 *  -1  failure
 */
int ui_setup(void);

/*
 * Restore terminal settings.
 */
void ui_restore(void);

/*
 * Hide/show terminal cursor.
 */
void ui_hide_cursor(void);
void ui_show_cursor(void);

/*
 * Clear the terminal screen and move the cursor home.
 */
void ui_clear(void);

/*
 * Read one keyboard key without blocking indefinitely.
 *
 * Returns:
 *   character value
 *   -1 when no key is available
 */
int ui_read_key(void);

/*
 * Wait for the requested amount of time while still allowing
 * interactive keyboard input.
 *
 * Returns:
 *   0  timeout expired
 *   key value when a key was pressed
 */
int ui_wait(double seconds);

/*
 * Check whether stdout/stdin are attached to a terminal.
 *
 * Returns:
 *   1  interactive terminal
 *   0  non-interactive
 */
int ui_is_interactive(void);

/*
 * Process interactive commands.
 *
 * Returns:
 *   1  continue monitoring
 *   0  quit
 */
int ui_handle_key(NeoConfig *config, int key, int *force_refresh);

#endif /* NEO_MONITORING_UI_H */
