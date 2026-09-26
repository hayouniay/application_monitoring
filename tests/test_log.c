#include "neo_test.h"

#include "monitoring_log.h"

#include <stdio.h>
#include <unistd.h>

/*
 * monitoring_log.c keeps its state in a single process-wide global, so
 * every test here resets it with log_init(NULL, ...) up front rather
 * than assuming a particular starting state, and each test file is its
 * own process (see tests/CMakeLists.txt) so there's no cross-file
 * interference either.
 */

/* ------------------------------------------------------------------------- */
/* log_level_parse / log_level_name                                         */
/* ------------------------------------------------------------------------- */

NEO_TEST(level_parse_recognizes_all_levels) {
  NeoLogLevel level;

  NEO_ASSERT_EQ(log_level_parse("error", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_ERROR);

  NEO_ASSERT_EQ(log_level_parse("warn", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_WARN);

  NEO_ASSERT_EQ(log_level_parse("warning", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_WARN);

  NEO_ASSERT_EQ(log_level_parse("info", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_INFO);

  NEO_ASSERT_EQ(log_level_parse("debug", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_DEBUG);
}

NEO_TEST(level_parse_is_case_insensitive) {
  NeoLogLevel level;

  NEO_ASSERT_EQ(log_level_parse("ERROR", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_ERROR);

  NEO_ASSERT_EQ(log_level_parse("DeBuG", &level), 0);
  NEO_ASSERT_EQ(level, NEO_LOG_DEBUG);
}

NEO_TEST(level_parse_rejects_unknown_text) {
  NeoLogLevel level = NEO_LOG_DEBUG;

  NEO_ASSERT_EQ(log_level_parse("verbose", &level), -1);
  /* Untouched on failure. */
  NEO_ASSERT_EQ(level, NEO_LOG_DEBUG);
}

NEO_TEST(level_parse_null_arguments_are_safe) {
  NeoLogLevel level;

  NEO_ASSERT_EQ(log_level_parse(NULL, &level), -1);
  NEO_ASSERT_EQ(log_level_parse("info", NULL), -1);
}

NEO_TEST(level_name_returns_expected_strings) {
  NEO_ASSERT_STREQ(log_level_name(NEO_LOG_ERROR), "error");
  NEO_ASSERT_STREQ(log_level_name(NEO_LOG_WARN), "warn");
  NEO_ASSERT_STREQ(log_level_name(NEO_LOG_INFO), "info");
  NEO_ASSERT_STREQ(log_level_name(NEO_LOG_DEBUG), "debug");
}

NEO_TEST(level_name_unknown_value_is_safe) {
  NEO_ASSERT_STREQ(log_level_name((NeoLogLevel)999), "?");
}

/* ------------------------------------------------------------------------- */
/* log_init / log_shutdown / log_file_enabled                               */
/* ------------------------------------------------------------------------- */

NEO_TEST(init_with_null_path_leaves_file_disabled) {
  log_init(NULL, NEO_LOG_INFO);

  NEO_ASSERT_FALSE(log_file_enabled());

  log_shutdown();
}

NEO_TEST(init_with_valid_path_enables_the_file) {
  char path[256];
  snprintf(path, sizeof(path), "/tmp/neo_test_log_%d.log", (int)getpid());
  remove(path);

  NEO_ASSERT_EQ(log_init(path, NEO_LOG_INFO), 0);
  NEO_ASSERT_TRUE(log_file_enabled());

  log_shutdown();
  NEO_ASSERT_FALSE(log_file_enabled());

  remove(path);
}

NEO_TEST(init_with_unwritable_path_fails_and_disables_file) {
  /* A directory that doesn't exist can never be opened for appending. */
  int result = log_init("/no/such/directory/app.log", NEO_LOG_INFO);

  NEO_ASSERT_EQ(result, -1);
  NEO_ASSERT_FALSE(log_file_enabled());

  log_shutdown();
}

NEO_TEST(init_can_be_called_repeatedly_to_change_level_or_path) {
  char path_a[256];
  char path_b[256];

  snprintf(path_a, sizeof(path_a), "/tmp/neo_test_log_a_%d.log", (int)getpid());
  snprintf(path_b, sizeof(path_b), "/tmp/neo_test_log_b_%d.log", (int)getpid());
  remove(path_a);
  remove(path_b);

  NEO_ASSERT_EQ(log_init(path_a, NEO_LOG_ERROR), 0);
  NEO_ASSERT_TRUE(log_file_enabled());

  NEO_ASSERT_EQ(log_init(path_b, NEO_LOG_DEBUG), 0);
  NEO_ASSERT_TRUE(log_file_enabled());

  log_shutdown();
  remove(path_a);
  remove(path_b);
}

NEO_TEST(shutdown_without_init_is_safe) {
  log_shutdown();
  log_shutdown();
}

/* ------------------------------------------------------------------------- */
/* log_write + log_copy_recent (history / threshold behavior)               */
/* ------------------------------------------------------------------------- */

NEO_TEST(write_below_threshold_is_recorded) {
  NeoLogEntry entries[4];
  size_t count;

  log_init(NULL, NEO_LOG_WARN);

  log_write(NEO_LOG_ERROR, "disk failure on %s", "sda1");
  log_write(NEO_LOG_WARN, "queue depth high");
  /* Both ERROR and WARN are at-or-above the WARN threshold. */

  count = log_copy_recent(entries, 4);

  NEO_ASSERT_EQ(count, (size_t)2);
  NEO_ASSERT_STREQ(entries[0].message, "disk failure on sda1");
  NEO_ASSERT_EQ(entries[0].level, NEO_LOG_ERROR);
  NEO_ASSERT_STREQ(entries[1].message, "queue depth high");
  NEO_ASSERT_EQ(entries[1].level, NEO_LOG_WARN);

  log_shutdown();
}

/* log_write()'s history is a single process-wide ring buffer, so this
 * test can't assume it's empty (an earlier test in this file may have
 * already written into it) - instead it checks that the total entry
 * count is unchanged by the two filtered-out writes below. */
/* monitoring_log.c's ring buffer capacity (LOG_HISTORY_CAPACITY) is an
 * internal implementation detail, not exposed via monitoring_log.h, so
 * this just needs to be comfortably larger than that private value. */
#define TEST_LOG_HISTORY_PROBE_CAPACITY 1024

static size_t total_history_count(void) {
  static NeoLogEntry buf[TEST_LOG_HISTORY_PROBE_CAPACITY];
  return log_copy_recent(buf, TEST_LOG_HISTORY_PROBE_CAPACITY);
}

NEO_TEST(write_above_threshold_is_dropped) {
  size_t before;
  size_t after;

  log_init(NULL, NEO_LOG_WARN);

  before = total_history_count();

  log_write(NEO_LOG_INFO, "this should be dropped");
  log_write(NEO_LOG_DEBUG, "this too");

  after = total_history_count();

  NEO_ASSERT_EQ(after, before);

  log_shutdown();
}

NEO_TEST(write_formats_printf_style_arguments) {
  NeoLogEntry entries[1];

  log_init(NULL, NEO_LOG_DEBUG);

  log_write(NEO_LOG_INFO, "cpu at %d%% (pid %d)", 87, 4242);

  NEO_ASSERT_EQ(log_copy_recent(entries, 1), (size_t)1);
  NEO_ASSERT_STREQ(entries[0].message, "cpu at 87% (pid 4242)");

  log_shutdown();
}

NEO_TEST(write_null_format_is_safe) {
  log_init(NULL, NEO_LOG_DEBUG);

  log_write(NEO_LOG_INFO, NULL);

  log_shutdown();
}

NEO_TEST(write_truncates_overlong_messages) {
  NeoLogEntry entries[1];
  char huge[NEO_LOG_MESSAGE_MAX * 2];
  size_t i;

  for (i = 0; i < sizeof(huge) - 1; ++i) {
    huge[i] = 'x';
  }
  huge[sizeof(huge) - 1] = '\0';

  log_init(NULL, NEO_LOG_DEBUG);

  log_write(NEO_LOG_INFO, "%s", huge);

  NEO_ASSERT_EQ(log_copy_recent(entries, 1), (size_t)1);
  /* Must be NUL-terminated and fit within the fixed-size buffer. */
  NEO_ASSERT_TRUE(strlen(entries[0].message) < NEO_LOG_MESSAGE_MAX);

  log_shutdown();
}

NEO_TEST(copy_recent_respects_max_count) {
  NeoLogEntry entries[2];
  size_t count;

  log_init(NULL, NEO_LOG_DEBUG);

  log_write(NEO_LOG_INFO, "one");
  log_write(NEO_LOG_INFO, "two");
  log_write(NEO_LOG_INFO, "three");

  count = log_copy_recent(entries, 2);

  NEO_ASSERT_EQ(count, (size_t)2);
  /* Most recent two, oldest first. */
  NEO_ASSERT_STREQ(entries[0].message, "two");
  NEO_ASSERT_STREQ(entries[1].message, "three");

  log_shutdown();
}

NEO_TEST(copy_recent_null_or_zero_is_safe) {
  NeoLogEntry entries[1];

  log_init(NULL, NEO_LOG_DEBUG);
  log_write(NEO_LOG_INFO, "hello");

  NEO_ASSERT_EQ(log_copy_recent(NULL, 1), (size_t)0);
  NEO_ASSERT_EQ(log_copy_recent(entries, 0), (size_t)0);

  log_shutdown();
}

/* ------------------------------------------------------------------------- */
/* Sinks                                                                      */
/* ------------------------------------------------------------------------- */

static int g_sink_calls;
static NeoLogEntry g_sink_last_entry;
static void *g_sink_last_user_data;

static void test_sink(const NeoLogEntry *entry, void *user_data) {
  g_sink_calls++;
  g_sink_last_entry = *entry;
  g_sink_last_user_data = user_data;
}

NEO_TEST(sink_is_invoked_for_entries_that_pass_the_filter) {
  int marker = 7;

  g_sink_calls = 0;
  g_sink_last_user_data = NULL;

  log_init(NULL, NEO_LOG_INFO);
  NEO_ASSERT_EQ(log_add_sink(test_sink, &marker), 0);

  log_write(NEO_LOG_INFO, "hello sink");

  NEO_ASSERT_EQ(g_sink_calls, 1);
  NEO_ASSERT_STREQ(g_sink_last_entry.message, "hello sink");
  NEO_ASSERT_TRUE(g_sink_last_user_data == &marker);

  log_remove_sink(test_sink, &marker);
  log_shutdown();
}

NEO_TEST(sink_is_not_invoked_for_filtered_out_entries) {
  int marker = 1;

  g_sink_calls = 0;

  log_init(NULL, NEO_LOG_ERROR);
  log_add_sink(test_sink, &marker);

  log_write(NEO_LOG_DEBUG, "should not reach sink");

  NEO_ASSERT_EQ(g_sink_calls, 0);

  log_remove_sink(test_sink, &marker);
  log_shutdown();
}

NEO_TEST(sink_removed_is_no_longer_invoked) {
  int marker = 2;

  g_sink_calls = 0;

  log_init(NULL, NEO_LOG_INFO);
  log_add_sink(test_sink, &marker);
  log_remove_sink(test_sink, &marker);

  log_write(NEO_LOG_INFO, "after removal");

  NEO_ASSERT_EQ(g_sink_calls, 0);

  log_shutdown();
}

NEO_TEST(add_sink_null_function_is_rejected) {
  NEO_ASSERT_EQ(log_add_sink(NULL, NULL), -1);
}

NEO_TEST(add_sink_fails_once_table_is_full) {
  int i;
  int result = 0;
  int markers[NEO_LOG_MAX_SINKS + 1];

  for (i = 0; i < NEO_LOG_MAX_SINKS + 1; ++i) {
    markers[i] = i;
    result = log_add_sink(test_sink, &markers[i]);
  }

  /* The last insert should have failed since the table only holds
   * NEO_LOG_MAX_SINKS entries. */
  NEO_ASSERT_EQ(result, -1);

  for (i = 0; i < NEO_LOG_MAX_SINKS; ++i) {
    log_remove_sink(test_sink, &markers[i]);
  }
}

NEO_TEST(remove_sink_matches_on_both_function_and_user_data) {
  int marker_a = 1;
  int marker_b = 2;

  g_sink_calls = 0;

  log_init(NULL, NEO_LOG_INFO);
  log_add_sink(test_sink, &marker_a);
  log_add_sink(test_sink, &marker_b);

  /* Removing with a different user_data must not remove marker_a's
   * registration. */
  log_remove_sink(test_sink, &marker_b);

  log_write(NEO_LOG_INFO, "still registered");

  NEO_ASSERT_EQ(g_sink_calls, 1);
  NEO_ASSERT_TRUE(g_sink_last_user_data == &marker_a);

  log_remove_sink(test_sink, &marker_a);
  log_shutdown();
}

NEO_TEST(remove_sink_not_registered_is_a_safe_no_op) {
  int marker = 42;
  log_remove_sink(test_sink, &marker);
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(level_parse_recognizes_all_levels);
  NEO_RUN(level_parse_is_case_insensitive);
  NEO_RUN(level_parse_rejects_unknown_text);
  NEO_RUN(level_parse_null_arguments_are_safe);
  NEO_RUN(level_name_returns_expected_strings);
  NEO_RUN(level_name_unknown_value_is_safe);

  NEO_RUN(init_with_null_path_leaves_file_disabled);
  NEO_RUN(init_with_valid_path_enables_the_file);
  NEO_RUN(init_with_unwritable_path_fails_and_disables_file);
  NEO_RUN(init_can_be_called_repeatedly_to_change_level_or_path);
  NEO_RUN(shutdown_without_init_is_safe);

  NEO_RUN(write_below_threshold_is_recorded);
  NEO_RUN(write_above_threshold_is_dropped);
  NEO_RUN(write_formats_printf_style_arguments);
  NEO_RUN(write_null_format_is_safe);
  NEO_RUN(write_truncates_overlong_messages);
  NEO_RUN(copy_recent_respects_max_count);
  NEO_RUN(copy_recent_null_or_zero_is_safe);

  NEO_RUN(sink_is_invoked_for_entries_that_pass_the_filter);
  NEO_RUN(sink_is_not_invoked_for_filtered_out_entries);
  NEO_RUN(sink_removed_is_no_longer_invoked);
  NEO_RUN(add_sink_null_function_is_rejected);
  NEO_RUN(add_sink_fails_once_table_is_full);
  NEO_RUN(remove_sink_matches_on_both_function_and_user_data);
  NEO_RUN(remove_sink_not_registered_is_a_safe_no_op);
NEO_TEST_MAIN_END()
