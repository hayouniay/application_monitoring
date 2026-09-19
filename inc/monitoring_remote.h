#ifndef MONITORING_REMOTE_H
#define MONITORING_REMOTE_H

#include "monitoring_services.h"

#define REMOTE_HOST_MAX MAX_HOST
#define REMOTE_USER_MAX MAX_USER
#define REMOTE_PATH_MAX MAX_REMOTE_PATH
#define REMOTE_MESSAGE_MAX 512

/*
 * Login target for ssh/telnet remote monitoring.
 */
typedef struct {
  char host[REMOTE_HOST_MAX];
  char user[REMOTE_USER_MAX];
  char password[REMOTE_USER_MAX];      /* telnet only; ssh uses keys */
  int port;                            /* 0 = protocol default */
  char identity_file[REMOTE_PATH_MAX]; /* ssh only, optional */
  char remote_binary[REMOTE_PATH_MAX]; /* CLI tool name/path on the card */
} NeoRemoteTarget;

/*
 * FTP deploy target.
 */
typedef struct {
  char host[REMOTE_HOST_MAX];
  char user[REMOTE_USER_MAX];
  char password[REMOTE_USER_MAX];
  int port; /* 0 = default 21 */
  char remote_filename[REMOTE_PATH_MAX];
} NeoFtpTarget;

/*
 * TFTP deploy target (no authentication).
 */
typedef struct {
  char host[REMOTE_HOST_MAX];
  int port; /* 0 = default 69 */
  char remote_filename[REMOTE_PATH_MAX];
} NeoTftpTarget;

void remote_target_init(NeoRemoteTarget *target);
void ftp_target_init(NeoFtpTarget *target);
void tftp_target_init(NeoTftpTarget *target);

/*
 * Connects over SSH (key-based, non-interactive), runs the monitoring
 * CLI on the card in one-shot CSV mode, and parses the result into
 * `list` (already initialized with process_list_init()). Also fills
 * `system` (if non-NULL) with the card's total memory and CPU count,
 * fetched in the same round trip. Requires an `ssh` client locally
 * and the remote binary already on the card.
 *
 * Returns 0 on success; -1 on failure with a reason in `message`.
 */
int remote_ssh_scan(const NeoRemoteTarget *target, NeoProcessList *list,
                    NeoSystemInfo *system, char *message, size_t message_size);

/*
 * Same as remote_ssh_scan(), but drives the system `telnet` client
 * with a scripted login (username/password if set) instead of ssh.
 * Also fills `system` (if non-NULL) with the card's total memory and
 * CPU count, fetched in the same session. Telnet automation is
 * inherently best-effort: it assumes a simple login prompt followed
 * by a shell, with no exotic terminal negotiation. Prefer SSH when
 * the card supports it.
 */
int remote_telnet_scan(const NeoRemoteTarget *target, NeoProcessList *list,
                       NeoSystemInfo *system, char *message,
                       size_t message_size);

/*
 * Lightweight SSH reachability check: logs in and checks whether the
 * configured remote binary can be found on the card, without doing a
 * full scan. Returns 0 if the SSH connection succeeded (check
 * `binary_found` separately), -1 if the connection itself failed.
 */
int remote_ssh_check(const NeoRemoteTarget *target, bool *binary_found,
                     char *message, size_t message_size);

/*
 * Uploads `local_path` to the card via FTP (using the system `curl`),
 * authenticating with `target->user`/`target->password` if set, or
 * anonymously otherwise.
 */
int remote_ftp_deploy(const NeoFtpTarget *target, const char *local_path,
                      char *message, size_t message_size);

/*
 * Uploads `local_path` to the card via TFTP put (using the system
 * `tftp` client). TFTP has no authentication.
 */
int remote_tftp_deploy(const NeoTftpTarget *target, const char *local_path,
                       char *message, size_t message_size);

#endif /* MONITORING_REMOTE_H */
