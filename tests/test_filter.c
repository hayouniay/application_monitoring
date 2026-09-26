#include "neo_test.h"

#include "monitoring_filter.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static NeoProcess make_process(pid_t pid, pid_t ppid, uid_t uid,
                               const char *user, char state,
                               const char *comm, const char *cmdline) {
  NeoProcess process;
  memset(&process, 0, sizeof(process));

  process.pid = pid;
  process.ppid = ppid;
  process.uid = uid;
  process.state = state;

  snprintf(process.user, sizeof(process.user), "%s", user);
  snprintf(process.comm, sizeof(process.comm), "%s", comm);
  snprintf(process.cmdline, sizeof(process.cmdline), "%s", cmdline);

  return process;
}

static void set_patterns(char ***patterns, size_t *count, const char *value) {
  *patterns = malloc(sizeof(char *));
  (*patterns)[0] = strdup(value);
  *count = 1;
}

static void free_patterns(char **patterns, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    free(patterns[i]);
  }
  free(patterns);
}

/* ------------------------------------------------------------------------- */
/* filter_contains_ci                                                        */
/* ------------------------------------------------------------------------- */

NEO_TEST(contains_ci_matches_regardless_of_case) {
  NEO_ASSERT_TRUE(filter_contains_ci("Firefox --new-window", "FOX"));
}

NEO_TEST(contains_ci_no_match) {
  NEO_ASSERT_FALSE(filter_contains_ci("firefox", "chrome"));
}

NEO_TEST(contains_ci_empty_pattern_always_matches) {
  NEO_ASSERT_TRUE(filter_contains_ci("anything", ""));
}

NEO_TEST(contains_ci_null_arguments_are_safe) {
  NEO_ASSERT_FALSE(filter_contains_ci(NULL, "x"));
  NEO_ASSERT_FALSE(filter_contains_ci("x", NULL));
}

/* ------------------------------------------------------------------------- */
/* filter_match_patterns / filter_match_excludes                             */
/* ------------------------------------------------------------------------- */

NEO_TEST(match_patterns_no_patterns_matches_everything) {
  NeoConfig config;
  NeoProcess process = make_process(1, 0, 0, "root", 'S', "init", "init");
  memset(&config, 0, sizeof(config));

  NEO_ASSERT_TRUE(filter_match_patterns(&config, &process));
}

NEO_TEST(match_patterns_matches_on_comm) {
  NeoConfig config;
  NeoProcess process =
      make_process(42, 1, 1000, "alice", 'R', "sshd", "/usr/sbin/sshd");
  memset(&config, 0, sizeof(config));
  set_patterns(&config.patterns, &config.pattern_count, "ssh");

  NEO_ASSERT_TRUE(filter_match_patterns(&config, &process));

  free_patterns(config.patterns, config.pattern_count);
}

NEO_TEST(match_patterns_rejects_when_no_pattern_matches) {
  NeoConfig config;
  NeoProcess process =
      make_process(42, 1, 1000, "alice", 'R', "sshd", "/usr/sbin/sshd");
  memset(&config, 0, sizeof(config));
  set_patterns(&config.patterns, &config.pattern_count, "nginx");

  NEO_ASSERT_FALSE(filter_match_patterns(&config, &process));

  free_patterns(config.patterns, config.pattern_count);
}

NEO_TEST(match_excludes_rejects_matching_process) {
  NeoConfig config;
  NeoProcess process =
      make_process(42, 1, 1000, "alice", 'R', "sshd", "/usr/sbin/sshd");
  memset(&config, 0, sizeof(config));
  set_patterns(&config.exclude_patterns, &config.exclude_count, "sshd");

  NEO_ASSERT_TRUE(filter_match_excludes(&config, &process));

  free_patterns(config.exclude_patterns, config.exclude_count);
}

NEO_TEST(process_matches_exclude_has_priority_over_include) {
  NeoConfig config;
  NeoProcess process =
      make_process(42, 1, 1000, "alice", 'R', "sshd", "/usr/sbin/sshd");
  memset(&config, 0, sizeof(config));

  set_patterns(&config.patterns, &config.pattern_count, "ssh");
  set_patterns(&config.exclude_patterns, &config.exclude_count, "sshd");

  /* Matches the include pattern, but the exclude pattern must win. */
  NEO_ASSERT_FALSE(process_matches(&config, &process));

  free_patterns(config.patterns, config.pattern_count);
  free_patterns(config.exclude_patterns, config.exclude_count);
}

/* ------------------------------------------------------------------------- */
/* filter_match_pid / filter_match_ppid                                      */
/* ------------------------------------------------------------------------- */

NEO_TEST(match_pid_no_filter_matches_everything) {
  NeoConfig config;
  NeoProcess process = make_process(99, 1, 0, "root", 'S', "x", "x");
  memset(&config, 0, sizeof(config));

  NEO_ASSERT_TRUE(filter_match_pid(&config, &process));
}

NEO_TEST(match_pid_matches_only_listed_pids) {
  NeoConfig config;
  pid_t pids[] = {10, 20, 30};
  NeoProcess matching = make_process(20, 1, 0, "root", 'S', "x", "x");
  NeoProcess not_matching = make_process(21, 1, 0, "root", 'S', "x", "x");

  memset(&config, 0, sizeof(config));
  config.pids = pids;
  config.pid_count = 3;

  NEO_ASSERT_TRUE(filter_match_pid(&config, &matching));
  NEO_ASSERT_FALSE(filter_match_pid(&config, &not_matching));
}

NEO_TEST(match_ppid_matches_only_listed_ppids) {
  NeoConfig config;
  pid_t ppids[] = {1};
  NeoProcess matching = make_process(20, 1, 0, "root", 'S', "x", "x");
  NeoProcess not_matching = make_process(21, 500, 0, "root", 'S', "x", "x");

  memset(&config, 0, sizeof(config));
  config.ppids = ppids;
  config.ppid_count = 1;

  NEO_ASSERT_TRUE(filter_match_ppid(&config, &matching));
  NEO_ASSERT_FALSE(filter_match_ppid(&config, &not_matching));
}

/* ------------------------------------------------------------------------- */
/* filter_match_user                                                         */
/* ------------------------------------------------------------------------- */

NEO_TEST(match_user_by_uid) {
  NeoConfig config;
  NeoProcess process = make_process(1, 0, 1000, "alice", 'S', "x", "x");
  memset(&config, 0, sizeof(config));
  config.filter_uid_enabled = true;
  config.filter_uid = 1000;

  NEO_ASSERT_TRUE(filter_match_user(&config, &process));

  config.filter_uid = 1001;
  NEO_ASSERT_FALSE(filter_match_user(&config, &process));
}

NEO_TEST(match_user_by_name_is_case_insensitive) {
  NeoConfig config;
  NeoProcess process = make_process(1, 0, 1000, "Alice", 'S', "x", "x");
  memset(&config, 0, sizeof(config));
  config.filter_user_enabled = true;
  snprintf(config.filter_user, sizeof(config.filter_user), "alice");

  NEO_ASSERT_TRUE(filter_match_user(&config, &process));
}

/* ------------------------------------------------------------------------- */
/* filter_match_state                                                        */
/* ------------------------------------------------------------------------- */

NEO_TEST(match_state_no_filter_matches_everything) {
  NeoConfig config;
  NeoProcess process = make_process(1, 0, 0, "root", 'Z', "x", "x");
  memset(&config, 0, sizeof(config));

  NEO_ASSERT_TRUE(filter_match_state(&config, &process));
}

NEO_TEST(match_state_matches_only_listed_states) {
  NeoConfig config;
  NeoProcess running = make_process(1, 0, 0, "root", 'R', "x", "x");
  NeoProcess zombie = make_process(2, 0, 0, "root", 'Z', "x", "x");

  memset(&config, 0, sizeof(config));
  config.states[0] = 'R';
  config.states[1] = 'S';
  config.state_count = 2;

  NEO_ASSERT_TRUE(filter_match_state(&config, &running));
  NEO_ASSERT_FALSE(filter_match_state(&config, &zombie));
}

/* ------------------------------------------------------------------------- */
/* NULL-safety                                                               */
/* ------------------------------------------------------------------------- */

NEO_TEST(all_filters_reject_null_arguments) {
  NeoConfig config;
  NeoProcess process = make_process(1, 0, 0, "root", 'R', "x", "x");
  memset(&config, 0, sizeof(config));

  NEO_ASSERT_FALSE(process_matches(NULL, &process));
  NEO_ASSERT_FALSE(process_matches(&config, NULL));
  NEO_ASSERT_FALSE(filter_match_patterns(NULL, &process));
  NEO_ASSERT_FALSE(filter_match_user(NULL, &process));
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(contains_ci_matches_regardless_of_case);
  NEO_RUN(contains_ci_no_match);
  NEO_RUN(contains_ci_empty_pattern_always_matches);
  NEO_RUN(contains_ci_null_arguments_are_safe);

  NEO_RUN(match_patterns_no_patterns_matches_everything);
  NEO_RUN(match_patterns_matches_on_comm);
  NEO_RUN(match_patterns_rejects_when_no_pattern_matches);
  NEO_RUN(match_excludes_rejects_matching_process);
  NEO_RUN(process_matches_exclude_has_priority_over_include);

  NEO_RUN(match_pid_no_filter_matches_everything);
  NEO_RUN(match_pid_matches_only_listed_pids);
  NEO_RUN(match_ppid_matches_only_listed_ppids);

  NEO_RUN(match_user_by_uid);
  NEO_RUN(match_user_by_name_is_case_insensitive);

  NEO_RUN(match_state_no_filter_matches_everything);
  NEO_RUN(match_state_matches_only_listed_states);

  NEO_RUN(all_filters_reject_null_arguments);
NEO_TEST_MAIN_END()
