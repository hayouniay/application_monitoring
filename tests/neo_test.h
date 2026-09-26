#ifndef NEO_TEST_H
#define NEO_TEST_H

/*
 * Minimal, dependency-free unit test helper.
 *
 * The project intentionally avoids pulling in an external test
 * framework (Unity/Criterion/etc.) so that `make test` / CTest keep
 * working offline with nothing beyond the toolchain already required
 * to build the CLI (see docs/REMOTE_TESTING.md for the same
 * "no extra moving parts" philosophy applied to remote testing).
 *
 * Usage, one executable per test file:
 *
 *   #include "neo_test.h"
 *   #include "monitoring_metrics.h"
 *
 *   NEO_TEST(kb_to_mb_converts) {
 *     NEO_ASSERT_NEAR(metrics_kb_to_mb(2048), 2.0, 0.0001);
 *   }
 *
 *   NEO_TEST_MAIN_BEGIN()
 *     NEO_RUN(kb_to_mb_converts);
 *   NEO_TEST_MAIN_END()
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

static int neo_test_count = 0;
static int neo_test_failures = 0;
static const char *neo_test_current = "";

#define NEO_TEST(name) static void name(void)

#define NEO_RUN(name)                                                       \
  do {                                                                      \
    neo_test_current = #name;                                              \
    neo_test_count++;                                                      \
    name();                                                                \
  } while (0)

#define NEO_ASSERT_TRUE(cond)                                               \
  do {                                                                      \
    if (!(cond)) {                                                         \
      printf("  FAIL %s (%s:%d): expected true: %s\n", neo_test_current,   \
             __FILE__, __LINE__, #cond);                                   \
      neo_test_failures++;                                                 \
    }                                                                       \
  } while (0)

#define NEO_ASSERT_FALSE(cond) NEO_ASSERT_TRUE(!(cond))

#define NEO_ASSERT_EQ(actual, expected)                                     \
  do {                                                                      \
    if ((actual) != (expected)) {                                          \
      printf("  FAIL %s (%s:%d): %s != %s\n", neo_test_current, __FILE__,  \
             __LINE__, #actual, #expected);                                \
      neo_test_failures++;                                                 \
    }                                                                       \
  } while (0)

#define NEO_ASSERT_STREQ(actual, expected)                                  \
  do {                                                                      \
    if (strcmp((actual), (expected)) != 0) {                               \
      printf("  FAIL %s (%s:%d): \"%s\" != \"%s\"\n", neo_test_current,    \
             __FILE__, __LINE__, (actual), (expected));                    \
      neo_test_failures++;                                                 \
    }                                                                       \
  } while (0)

#define NEO_ASSERT_NEAR(actual, expected, epsilon)                          \
  do {                                                                      \
    double neo_a_ = (double)(actual);                                      \
    double neo_b_ = (double)(expected);                                    \
    if (fabs(neo_a_ - neo_b_) > (epsilon)) {                               \
      printf("  FAIL %s (%s:%d): %s (%.6f) != %s (%.6f)\n",                \
             neo_test_current, __FILE__, __LINE__, #actual, neo_a_,        \
             #expected, neo_b_);                                           \
      neo_test_failures++;                                                 \
    }                                                                       \
  } while (0)

#define NEO_TEST_MAIN_BEGIN()                                               \
  int main(void) {

#define NEO_TEST_MAIN_END()                                                 \
  if (neo_test_failures == 0) {                                            \
    printf("PASS: %d test(s)\n", neo_test_count);                          \
  } else {                                                                  \
    printf("FAILED: %d/%d test(s)\n", neo_test_failures, neo_test_count);  \
  }                                                                          \
  return neo_test_failures == 0 ? 0 : 1;                                   \
  }

#endif /* NEO_TEST_H */
