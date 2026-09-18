#ifndef MONITORING_PROCESS_H
#define MONITORING_PROCESS_H

#include "monitoring_services.h"

int parse_pid_path(const char *name, pid_t *pid);

int process_collect(pid_t pid, NeoProcess *process);

int process_read_cmdline(pid_t pid, char *buffer, size_t size);

int process_read_io(pid_t pid, unsigned long long *read_bytes,
                    unsigned long long *write_bytes);

int process_read_status(pid_t pid, uid_t *uid, char *user, size_t user_size,
                        unsigned long long *rss_kb, unsigned long long *swap_kb,
                        unsigned long *threads);

int process_read_stat(pid_t pid, NeoProcess *process);

int process_get_start_time(pid_t pid, time_t *start_time);

#endif /* MONITORING_PROCESS_H */
