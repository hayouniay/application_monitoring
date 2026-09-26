cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NEO_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${NEO_VERSION_STRING}" NEO_VERSION_STRING)

project(
    app_top_monitoring
    VERSION ${NEO_VERSION_STRING}
    DESCRIPTION "A top-like Linux process monitor"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_QT_APP "Build the Qt GUI application" ON)
option(BUILD_TESTS "Build unit tests for monitoring_core" ON)

find_package(Threads REQUIRED)

if(BUILD_QT_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

set(CORE_SOURCES
    src/monitoring_services_core.c
    src/monitoring_process.c
    src/monitoring_process_control.c
    src/monitoring_metrics.c
    src/monitoring_filter.c
    src/monitoring_output.c
    src/monitoring_remote.c
    src/monitoring_capture.c
    src/monitoring_log.c
    src/monitoring_alert.c
    src/monitoring_targets.c
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/monitoring_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/monitoring_version.h
    @ONLY
)

string(TIMESTAMP NEO_MAN_DATE "%Y-%m-%d" UTC)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/app_top_monitoring.1.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1
    @ONLY
)

add_library(
    monitoring_core
    STATIC
    ${CORE_SOURCES}
)

target_include_directories(
    monitoring_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_compile_options(
    monitoring_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(
    monitoring_core
    PUBLIC
        Threads::Threads
)

add_executable(
    app_top_monitoring

    src/monotoring_services.c
    src/monitoring_ui.c
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

target_link_libraries(
    app_top_monitoring
    PRIVATE
        monitoring_core
)

if(BUILD_QT_APP)
    qt_add_executable(
        app_top_monitoring_qt

        src/qt/qt_application.cpp
        src/qt/qt_main_window.cpp
        src/qt/qt_process_model.cpp
        src/qt/qt_monitor_controller.cpp
        src/qt/qt_settings_dialog.cpp
        src/qt/qt_theme.cpp
        src/qt/qt_remote_dialog.cpp
        src/qt/qt_wave_widget.cpp
        src/qt/qt_graphs_window.cpp
        src/qt/qt_capture_window.cpp
        src/qt/qt_log_window.cpp

        inc/qt/qt_application.h
        inc/qt/qt_main_window.h
        inc/qt/qt_process_model.h
        inc/qt/qt_monitor_controller.h
        inc/qt/qt_settings_dialog.h
        inc/qt/qt_theme.h
        inc/qt/qt_remote_dialog.h
        inc/qt/qt_wave_widget.h
        inc/qt/qt_graphs_window.h
        inc/qt/qt_capture_window.h
        inc/qt/qt_log_window.h

        ui/qt/resources/monitoring.qrc
    )

    target_include_directories(
        app_top_monitoring_qt
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/inc
    )

    target_compile_options(
        app_top_monitoring_qt
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_link_libraries(
        app_top_monitoring_qt
        PRIVATE
            monitoring_core
            Qt6::Widgets
            Threads::Threads
    )

    set_target_properties(
        app_top_monitoring_qt
        PROPERTIES
            AUTOMOC ON
            AUTOUIC ON
            AUTORCC ON
    )
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(
    TARGETS
        app_top_monitoring
    RUNTIME DESTINATION bin
)

if(BUILD_QT_APP)
    install(
        TARGETS
            app_top_monitoring_qt
        RUNTIME DESTINATION bin
    )
endif()

# ---------------------------------------------------------------------------
# Man page: docs/app_top_monitoring.1.in is configured above (version/date
# filled in, same pattern as monitoring_version.h.in) into the build tree.
# Compress it with gzip if available, since that's what most distros expect
# man pages to be, but fall back to installing it uncompressed - either way
# `man app_top_monitoring` works after `make install`.
# ---------------------------------------------------------------------------

find_program(GZIP_EXECUTABLE gzip)

if(GZIP_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1.gz
        COMMAND ${GZIP_EXECUTABLE} -9 -n -c
            ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1
            > ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1.gz
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1
        COMMENT "Compressing man page"
        VERBATIM
    )

    add_custom_target(
        man_page ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1.gz
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1.gz
        DESTINATION share/man/man1
    )
else()
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/app_top_monitoring.1
        DESTINATION share/man/man1
    )
endif()
