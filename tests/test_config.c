#include "neo_test.h"

#include "monitoring_services.h"

#include <stdlib.h>

/* ------------------------------------------------------------------------- */
/* config_init                                                               */
/* ------------------------------------------------------------------------- */

NEO_TEST(config_init_sets_expected_defaults) {
  NeoConfig config;
  config_init(&config);

  NEO_ASSERT_NEAR(config.interval, DEFAULT_INTERVAL, 0.0001);
  NEO_ASSERT_EQ(config.sort_mode, SORT_CPU);
  NEO_ASSERT_EQ(config.protocol, PROTOCOL_LOCAL);
  NEO_ASSERT_FALSE(config.once);
  NEO_ASSERT_FALSE(config.csv);
  NEO_ASSERT_FALSE(config.json);
  NEO_ASSERT_FALSE(config.filter_uid_enabled);
  NEO_ASSERT_FALSE(config.filter_user_enabled);
  NEO_ASSERT_EQ(config.pattern_count, (size_t)0);
  NEO_ASSERT_EQ(config.exclude_count, (size_t)0);
  NEO_ASSERT_EQ(config.pid_count, (size_t)0);

  config_free(&config);
}

NEO_TEST(config_init_null_is_safe) {
  config_init(NULL);
}

NEO_TEST(config_free_null_is_safe) {
  config_free(NULL);
}

NEO_TEST(config_free_is_safe_to_call_on_a_freshly_initialized_config) {
  NeoConfig config;
  config_init(&config);
  config_free(&config);

  NEO_ASSERT_EQ(config.patterns, NULL);
  NEO_ASSERT_EQ(config.pattern_count, (size_t)0);
}

/* ------------------------------------------------------------------------- */
/* process_list_init / process_list_free                                    */
/* ------------------------------------------------------------------------- */

NEO_TEST(process_list_init_zeroes_the_list) {
  NeoProcessList list;
  list.items = (NeoProcess *)0x1;
  list.count = 5;
  list.capacity = 5;

  process_list_init(&list);

  NEO_ASSERT_EQ(list.items, NULL);
  NEO_ASSERT_EQ(list.count, (size_t)0);
  NEO_ASSERT_EQ(list.capacity, (size_t)0);
}

NEO_TEST(process_list_free_null_is_safe) {
  process_list_free(NULL);
}

NEO_TEST(process_list_free_resets_the_list) {
  NeoProcessList list;
  process_list_init(&list);

  list.items = malloc(sizeof(NeoProcess) * 4);
  list.count = 4;
  list.capacity = 4;

  process_list_free(&list);

  NEO_ASSERT_EQ(list.items, NULL);
  NEO_ASSERT_EQ(list.count, (size_t)0);
  NEO_ASSERT_EQ(list.capacity, (size_t)0);
}

/* ------------------------------------------------------------------------- */
/* sort_processes                                                            */
/* ------------------------------------------------------------------------- */

static NeoProcessList build_list(void) {
  NeoProcessList list;
  process_list_init(&list);

  list.items = calloc(3, sizeof(NeoProcess));
  list.count = 3;
  list.capacity = 3;

  list.items[0].pid = 10;
  list.items[0].cpu_percent = 5.0;

  list.items[1].pid = 20;
  list.items[1].cpu_percent = 50.0;

  list.items[2].pid = 30;
  list.items[2].cpu_percent = 25.0;

  return list;
}

NEO_TEST(sort_by_cpu_orders_descending_by_default) {
  NeoProcessList list = build_list();

  sort_processes(&list, SORT_CPU, false);

  NEO_ASSERT_EQ(list.items[0].pid, 20);
  NEO_ASSERT_EQ(list.items[1].pid, 30);
  NEO_ASSERT_EQ(list.items[2].pid, 10);

  process_list_free(&list);
}

NEO_TEST(sort_by_cpu_reverse_flips_the_order) {
  NeoProcessList list = build_list();

  sort_processes(&list, SORT_CPU, true);

  NEO_ASSERT_EQ(list.items[0].pid, 10);
  NEO_ASSERT_EQ(list.items[1].pid, 30);
  NEO_ASSERT_EQ(list.items[2].pid, 20);

  process_list_free(&list);
}

NEO_TEST(sort_by_pid_orders_ascending) {
  NeoProcessList list = build_list();

  sort_processes(&list, SORT_PID, false);

  NEO_ASSERT_EQ(list.items[0].pid, 10);
  NEO_ASSERT_EQ(list.items[1].pid, 20);
  NEO_ASSERT_EQ(list.items[2].pid, 30);

  process_list_free(&list);
}

NEO_TEST(sort_processes_empty_list_is_safe) {
  NeoProcessList list;
  process_list_init(&list);

  sort_processes(&list, SORT_CPU, false);
  sort_processes(NULL, SORT_CPU, false);

  NEO_ASSERT_EQ(list.count, (size_t)0);
}

NEO_TEST(sort_processes_single_item_is_safe) {
  NeoProcessList list;
  process_list_init(&list);

  list.items = calloc(1, sizeof(NeoProcess));
  list.count = 1;
  list.capacity = 1;
  list.items[0].pid = 42;

  sort_processes(&list, SORT_CPU, false);

  NEO_ASSERT_EQ(list.items[0].pid, 42);

  process_list_free(&list);
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(config_init_sets_expected_defaults);
  NEO_RUN(config_init_null_is_safe);
  NEO_RUN(config_free_null_is_safe);
  NEO_RUN(config_free_is_safe_to_call_on_a_freshly_initialized_config);

  NEO_RUN(process_list_init_zeroes_the_list);
  NEO_RUN(process_list_free_null_is_safe);
  NEO_RUN(process_list_free_resets_the_list);

  NEO_RUN(sort_by_cpu_orders_descending_by_default);
  NEO_RUN(sort_by_cpu_reverse_flips_the_order);
  NEO_RUN(sort_by_pid_orders_ascending);
  NEO_RUN(sort_processes_empty_list_is_safe);
  NEO_RUN(sort_processes_single_item_is_safe);
NEO_TEST_MAIN_END()
