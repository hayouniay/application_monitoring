#include "neo_test.h"

#include "monitoring_alert.h"

static NeoConfig make_config(double cpu_threshold, double mem_threshold,
                             double sustain_seconds) {
  NeoConfig config;
  memset(&config, 0, sizeof(config));
  config.alert_cpu_percent = cpu_threshold;
  config.alert_mem_percent = mem_threshold;
  config.alert_sustain_seconds = sustain_seconds;
  return config;
}

/* ------------------------------------------------------------------------- */
/* alert_evaluate                                                            */
/* ------------------------------------------------------------------------- */

NEO_TEST(evaluate_fires_immediately_when_sustain_is_zero) {
  NeoConfig config = make_config(80.0, 0.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 95.0, 10.0);

  NEO_ASSERT_EQ(mask, NEO_ALERT_CPU);
  NEO_ASSERT_TRUE(alert_state_is_active(&state));
}

NEO_TEST(evaluate_does_not_fire_below_threshold) {
  NeoConfig config = make_config(80.0, 0.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 50.0, 10.0);

  NEO_ASSERT_EQ(mask, 0);
  NEO_ASSERT_FALSE(alert_state_is_active(&state));
}

NEO_TEST(evaluate_disabled_check_never_fires) {
  /* Threshold <= 0 disables the check entirely. */
  NeoConfig config = make_config(0.0, 0.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 100.0, 100.0);

  NEO_ASSERT_EQ(mask, 0);
  NEO_ASSERT_FALSE(alert_state_is_active(&state));
}

NEO_TEST(evaluate_does_not_fire_twice_for_the_same_sustained_breach) {
  NeoConfig config = make_config(80.0, 0.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int first = alert_evaluate(&config, &state, 95.0, 0.0);
  int second = alert_evaluate(&config, &state, 95.0, 0.0);

  NEO_ASSERT_EQ(first, NEO_ALERT_CPU);
  NEO_ASSERT_EQ(second, 0);
  /* Still considered active even though it already fired once. */
  NEO_ASSERT_TRUE(alert_state_is_active(&state));
}

NEO_TEST(evaluate_requires_sustain_window_before_firing) {
  /* A large sustain window means a single instantaneous sample must
   * not fire yet, even though the value is over threshold. */
  NeoConfig config = make_config(80.0, 0.0, 3600.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 95.0, 0.0);

  NEO_ASSERT_EQ(mask, 0);
  NEO_ASSERT_TRUE(alert_state_is_active(&state));
}

NEO_TEST(evaluate_recovers_after_dropping_below_threshold) {
  NeoConfig config = make_config(80.0, 0.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  (void)alert_evaluate(&config, &state, 95.0, 0.0);
  int recovered = alert_evaluate(&config, &state, 50.0, 0.0);

  NEO_ASSERT_EQ(recovered, 0);
  NEO_ASSERT_FALSE(alert_state_is_active(&state));

  /* And it can fire again on a fresh breach. */
  int refired = alert_evaluate(&config, &state, 95.0, 0.0);
  NEO_ASSERT_EQ(refired, NEO_ALERT_CPU);
}

NEO_TEST(evaluate_cpu_and_memory_are_independent) {
  NeoConfig config = make_config(80.0, 80.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 95.0, 10.0);

  NEO_ASSERT_EQ(mask, NEO_ALERT_CPU);

  alert_state_init(&state);
  mask = alert_evaluate(&config, &state, 10.0, 95.0);

  NEO_ASSERT_EQ(mask, NEO_ALERT_MEM);
}

NEO_TEST(evaluate_both_can_fire_together) {
  NeoConfig config = make_config(80.0, 80.0, 0.0);
  NeoAlertState state;
  alert_state_init(&state);

  int mask = alert_evaluate(&config, &state, 95.0, 95.0);

  NEO_ASSERT_EQ(mask, NEO_ALERT_CPU | NEO_ALERT_MEM);
}

NEO_TEST(evaluate_null_arguments_are_safe) {
  NeoAlertState state;
  NeoConfig config = make_config(80.0, 0.0, 0.0);
  alert_state_init(&state);

  NEO_ASSERT_EQ(alert_evaluate(NULL, &state, 95.0, 0.0), 0);
  NEO_ASSERT_EQ(alert_evaluate(&config, NULL, 95.0, 0.0), 0);
}

NEO_TEST(state_is_active_null_state_is_safe) {
  NEO_ASSERT_FALSE(alert_state_is_active(NULL));
}

NEO_TEST_MAIN_BEGIN()
  NEO_RUN(evaluate_fires_immediately_when_sustain_is_zero);
  NEO_RUN(evaluate_does_not_fire_below_threshold);
  NEO_RUN(evaluate_disabled_check_never_fires);
  NEO_RUN(evaluate_does_not_fire_twice_for_the_same_sustained_breach);
  NEO_RUN(evaluate_requires_sustain_window_before_firing);
  NEO_RUN(evaluate_recovers_after_dropping_below_threshold);
  NEO_RUN(evaluate_cpu_and_memory_are_independent);
  NEO_RUN(evaluate_both_can_fire_together);
  NEO_RUN(evaluate_null_arguments_are_safe);
  NEO_RUN(state_is_active_null_state_is_safe);
NEO_TEST_MAIN_END()
