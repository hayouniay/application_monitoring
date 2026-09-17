cmake_minimum_required(VERSION 3.16)

project(
    app_top_monitoring
    VERSION 1.0.0
    DESCRIPTION "A top-like Linux process monitor"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(
    app_top_monitoring

    src/monotoring_services.c
    src/monitoring_services_core.c
    src/monitoring_process.c
    src/monitoring_metrics.c
    src/monitoring_filter.c
    src/monitoring_ui.c
    src/monitoring_output.c
)

target_include_directories(
    app_top_monitoring
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_options(
    app_top_monitoring
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

install(
    TARGETS app_top_monitoring
    RUNTIME DESTINATION bin
)

