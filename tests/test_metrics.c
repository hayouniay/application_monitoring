#include "neo_test.h"

#include "monitoring_metrics.h"

/* ------------------------------------------------------------------------- */
/* metrics_cpu_percent                                                       */
/* ------------------------------------------------------------------------- */

NEO_TEST(cpu_percent_single_cpu_full_delta) {
  /* Process consumed the entire system delta on a single CPU: 100%. */
  double result = metrics_cpu_percent(100, 0, 100, 0, 1);
  NEO_ASSERT_NEAR(result, 100.0, 0.0001);
}

NEO_TEST(cpu_percent_half_system_delta) {
  double result = metrics_cpu_percent(50, 0, 100, 0, 1);
  NEO_ASSERT_NEAR(result, 50.0, 0.0001);
}

NEO_TEST(cpu_percent_scales_with_cpu_count) {
  /* Same ratio, but on 4 CPUs the result should be 4x. */
  double result = metrics_cpu_percent(50, 0, 100, 0, 4);
  NEO_ASSERT_NEAR(result, 200.0, 0.0001);
}

NEO_TEST(cpu_percent_zero_cpu_count_treated_as_one) {
  double with_zero = metrics_cpu_percent(50, 0, 100, 0, 0);
  double with_one = metrics_cpu_percent(50, 0, 100, 0, 1);
  NEO_ASSERT_NEAR(with_zero, with_one, 0.0001);
}

NEO_TEST(cpu_percent_counter_wraparound_process_is_safe) {
  /* current_total < previous_total (e.g. counter reset) must not
   * underflow into a huge unsigned value. */
  double result = metrics_cpu_percent(5, 100, 200, 0, 1);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

NEO_TEST(cpu_percent_counter_wraparound_system_is_safe) {
  double result = metrics_cpu_percent(50, 0, 5, 100, 1);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

NEO_TEST(cpu_percent_zero_system_delta_is_safe) {
  /* No system time elapsed - must not divide by zero. */
  double result = metrics_cpu_percent(50, 0, 100, 100, 1);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

/* ------------------------------------------------------------------------- */
/* metrics_memory_percent                                                    */
/* ------------------------------------------------------------------------- */

NEO_TEST(memory_percent_basic_ratio) {
  double result = metrics_memory_percent(512, 2048);
  NEO_ASSERT_NEAR(result, 25.0, 0.0001);
}

NEO_TEST(memory_percent_zero_total_is_safe) {
  double result = metrics_memory_percent(512, 0);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

/* ------------------------------------------------------------------------- */
/* metrics_io_rate                                                           */
/* ------------------------------------------------------------------------- */

NEO_TEST(io_rate_basic) {
  /* 1 MiB delta over 1 second == 1 MB/s. */
  double result = metrics_io_rate(1024ULL * 1024ULL, 0, 1.0);
  NEO_ASSERT_NEAR(result, 1.0, 0.0001);
}

NEO_TEST(io_rate_zero_interval_is_safe) {
  double result = metrics_io_rate(1024ULL * 1024ULL, 0, 0.0);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

NEO_TEST(io_rate_negative_interval_is_safe) {
  double result = metrics_io_rate(1024ULL * 1024ULL, 0, -1.0);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

NEO_TEST(io_rate_counter_wraparound_is_safe) {
  double result = metrics_io_rate(10, 1000, 1.0);
  NEO_ASSERT_NEAR(result, 0.0, 0.0001);
}

/* ------------------------------------------------------------------------- */
/* metrics_kb_to_mb                                                          */
/* ------------------------------------------------------------------------- */

NEO_TEST(kb_to_mb_converts) {
  NEO_ASSERT_NEAR(metrics_kb_to_mb(2048), 2.0, 0.0001);
}

NEO_TEST(kb_to_mb_zero) {
  NEO_ASSERT_NEAR(metrics_kb_to_mb(0), 0.0, 0.0001);
}

/* ------------------------------------------------------------------------- */
/* metrics_elapsed_seconds                                                   */
/* ------------------------------------------------------------------------- */

NEO_TEST(elapsed_seconds_zero_start_time_is_safe) {
  NEO_ASSERT_NEAR(metrics_elapsed_seconds(0), 0.0, 0.0001);
}

NEO_TEST(elapsed_seconds_future_start_time_is_safe) {
  time_t far_future = time(NULL) + 3600;
  NEO_ASSERT_NEAR(metrics_elapsed_seconds(far_future), 0.0, 0.0001);
}

NEO_TEST(elapsed_seconds_past_start_time_is_positive) {
  time_t one_minute_ago = time(NULL) - 60;
  double elapsed = metrics_elapsed_seconds(one_minute_ago);
  NEO_ASSERT_TRUE(elapsed >= 59.0 && elapsed <= 61.0);
}

/* ------------------------------------------------------------------------- */
/* metrics_total_cpu_time                                                    */
/* ------------------------------------------------------------------------- */

NEO_TEST(total_cpu_time_sums_user_and_system) {
  NeoProcess process;
  memset(&process, 0, sizeof(process));
  process.utime = 30;
  process.stime = 12;

  NEO_ASSERT_EQ(metrics_total_cpu_time(&process), 42ULL);
}

NEO_TEST(total_cpu_time_null_process_is_safe) {
  NEO_ASSERT_EQ(metrics_total_cpu_time(NULL), 0ULL);
}

/* ------------------------------------------------------------------------- */
/* update_metrics                                                            */
/* ------------------------------------------------------------------------- */

NEO_TEST(update_metrics_null_process_is_safe) {
  /* Must not crash; nothing to assert beyond "does not crash". */
  update_metrics(NULL, NULL, NULL, 1.0);
}

NEO_TEST(update_metrics_computes_memory_and_unit_conversions) {
  NeoProcess process;
  NeoSystemInfo system;

  memset(&process, 0, sizeof(process));
  memset(&system, 0, sizeof(system));

  process.rss_kb = 1024;
  process.vsz_kb = 2048;
  process.swap_kb = 512;

  system.total_memory_kb = 4096;
  system.cpu_count = 1;

  update_metrics(&process, NULL, &system, 1.0);

  NEO_ASSERT_NEAR(process.mem_percent, 25.0, 0.0001);
  NEO_ASSERT_NEAR(process.rss_mb, 1.0, 0.0001);
  NEO_ASSERT_NEAR(process.vsz_mb, 2.0, 0.0001);
  NEO_ASSERT_NEAR(process.swap_mb, 0.5, 0.0001);
  NEO_ASSERT_NEAR(process.io_read_mb_s, 0.0, 0.0001);
  NEO_ASSERT_NEAR(process.io_write_mb_s, 0.0, 0.0001);
}

NEO_TEST(update_metrics_without_system_zeroes_memory_percent) {
  NeoProcess process;
  memset(&process, 0, sizeof(process));
  process.rss_kb = 1024;

  update_metrics(&process, NULL, NULL, 1.0);

  NEO_ASSERT_NEAR(process.mem_percent, 0.0, 0.0001);
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(cpu_percent_single_cpu_full_delta);
  NEO_RUN(cpu_percent_half_system_delta);
  NEO_RUN(cpu_percent_scales_with_cpu_count);
  NEO_RUN(cpu_percent_zero_cpu_count_treated_as_one);
  NEO_RUN(cpu_percent_counter_wraparound_process_is_safe);
  NEO_RUN(cpu_percent_counter_wraparound_system_is_safe);
  NEO_RUN(cpu_percent_zero_system_delta_is_safe);

  NEO_RUN(memory_percent_basic_ratio);
  NEO_RUN(memory_percent_zero_total_is_safe);

  NEO_RUN(io_rate_basic);
  NEO_RUN(io_rate_zero_interval_is_safe);
  NEO_RUN(io_rate_negative_interval_is_safe);
  NEO_RUN(io_rate_counter_wraparound_is_safe);

  NEO_RUN(kb_to_mb_converts);
  NEO_RUN(kb_to_mb_zero);

  NEO_RUN(elapsed_seconds_zero_start_time_is_safe);
  NEO_RUN(elapsed_seconds_future_start_time_is_safe);
  NEO_RUN(elapsed_seconds_past_start_time_is_positive);

  NEO_RUN(total_cpu_time_sums_user_and_system);
  NEO_RUN(total_cpu_time_null_process_is_safe);

  NEO_RUN(update_metrics_null_process_is_safe);
  NEO_RUN(update_metrics_computes_memory_and_unit_conversions);
  NEO_RUN(update_metrics_without_system_zeroes_memory_percent);
NEO_TEST_MAIN_END()
