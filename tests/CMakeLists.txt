# ---------------------------------------------------------------------------
# Unit tests for monitoring_core (pure C, no Qt / no /proc access required).
#
# Each test_*.c file builds into its own tiny executable and is registered
# as its own CTest case, so a failure points straight at the failing file
# and `ctest -R <name>` can re-run just that group.
# ---------------------------------------------------------------------------

set(NEO_TEST_SOURCES
    test_metrics.c
    test_filter.c
    test_alert.c
    test_config.c
    test_target.c
    test_log.c
)

foreach(test_source ${NEO_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_link_libraries(${test_name} PRIVATE monitoring_core)

    target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
