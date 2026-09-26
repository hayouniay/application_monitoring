#ifndef MONITORING_TARGETS_H
#define MONITORING_TARGETS_H

#include <stddef.h>

#include "monitoring_services.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* A single saved target                                                     */
/* ------------------------------------------------------------------------- */

/* TARGET_NAME_MAX is defined in monitoring_services.h (included above)
 * to avoid a circular include, since NeoConfig also needs it. */

typedef struct {
  /* Unique key used to look the target up (--target NAME / the Qt
   * dialog's dropdown). Comparison is case-sensitive. */
  char name[TARGET_NAME_MAX];

  /* PROTOCOL_SSH, PROTOCOL_TELNET, PROTOCOL_FTP or PROTOCOL_TFTP.
   * PROTOCOL_LOCAL is accepted but meaningless (there is nothing to
   * save for local monitoring). */
  NeoProtocol protocol;

  char host[MAX_HOST];
  char user[MAX_USER];

  /*
   * Stored in plain text, same as --password on the command line -
   * this file is only as safe as its filesystem permissions
   * (targets_save() writes it 0600). Telnet/FTP only; SSH always uses
   * key-based auth. Leave empty and rely on --password at the prompt
   * if you'd rather not persist it at all.
   */
  char password[MAX_USER];

  int port; /* 0 = protocol default */

  char identity[MAX_REMOTE_PATH];    /* SSH private key path */
  char remote_bin[MAX_REMOTE_PATH];  /* ssh/telnet monitoring binary */
  char remote_file[MAX_REMOTE_PATH]; /* ftp/tftp deploy destination name */
} NeoTarget;

void target_init(NeoTarget *target);

/* ------------------------------------------------------------------------- */
/* A list of saved targets, backed by the config file                        */
/* ------------------------------------------------------------------------- */

typedef struct {
  NeoTarget *items;
  size_t count;
  size_t capacity;
} NeoTargetList;

void target_list_init(NeoTargetList *list);
void target_list_free(NeoTargetList *list);

/*
 * Finds a target by name. Returns a pointer into `list` (valid until
 * the list is next modified/freed), or NULL if there is no target with
 * that name.
 */
const NeoTarget *target_list_find(const NeoTargetList *list, const char *name);

/*
 * Inserts a copy of `target`, or - if a target with the same name
 * already exists - overwrites it in place. Returns 0 on success, -1 on
 * allocation failure.
 */
int target_list_upsert(NeoTargetList *list, const NeoTarget *target);

/*
 * Removes the target named `name`. Returns 0 if it was found and
 * removed, -1 if no such target existed.
 */
int target_list_remove(NeoTargetList *list, const char *name);

/* ------------------------------------------------------------------------- */
/* Persistence (~/.config/neo-monitoring/targets.json by default)            */
/* ------------------------------------------------------------------------- */

/*
 * Fills `buffer` with the default targets file path: honors
 * $XDG_CONFIG_HOME (falling back to "$HOME/.config") plus
 * "/neo-monitoring/targets.json". Returns 0 on success, -1 if neither
 * $XDG_CONFIG_HOME nor $HOME/getpwuid() could resolve a home
 * directory, or the result wouldn't fit in `buffer`.
 */
int targets_default_path(char *buffer, size_t buffer_size);

/*
 * Loads every saved target from `path` (or the default path, if `path`
 * is NULL/empty) into `list`, which must already be initialized (see
 * target_list_init()) - existing entries are cleared first. A missing
 * file is treated as "no saved targets yet" rather than an error, so
 * this is always safe to call unconditionally on startup. Returns 0 on
 * success (including "file doesn't exist"), -1 on a real I/O error or
 * if the file exists but isn't valid JSON in the expected shape.
 */
int targets_load(const char *path, NeoTargetList *list);

/*
 * Writes every target in `list` to `path` (or the default path),
 * creating the containing directory (and its parent) if needed, and
 * restricting the file to user-only read/write (0600) since it may
 * contain a plaintext telnet/FTP password. Returns 0 on success, -1 on
 * failure (directory couldn't be created, file couldn't be written).
 */
int targets_save(const char *path, const NeoTargetList *list);

/* ------------------------------------------------------------------------- */
/* Bridging to/from NeoConfig                                                */
/* ------------------------------------------------------------------------- */

/*
 * Copies the remote-connection fields (protocol, host, user, password,
 * port, identity, remote_bin, remote_file) out of `config` into `out`,
 * naming the result `name`. Used to implement --save-target /
 * "Save as..." in the Qt dialog.
 */
void target_from_config(const NeoConfig *config, const char *name,
                        NeoTarget *out);

/*
 * Copies `target`'s fields onto the matching remote-connection fields
 * of `config`. Intended to be applied as *defaults*: call this before
 * (or have it overwritten by) any explicit --host/--user/etc. flags,
 * so a saved target pre-fills a connection but an explicit flag on the
 * same command line still wins.
 */
void target_apply_to_config(const NeoTarget *target, NeoConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* MONITORING_TARGETS_H */
