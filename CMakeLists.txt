cmake_minimum_required(VERSION 3.16)

project(
    app_top_monitoring
    VERSION 1.0.0
    DESCRIPTION "A top-like Linux process monitor"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(BUILD_QT_APP "Build the Qt GUI application" ON)

if(BUILD_QT_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    find_package(Threads REQUIRED)
endif()

set(CORE_SOURCES
    src/monitoring_services_core.c
    src/monitoring_process.c
    src/monitoring_metrics.c
    src/monitoring_filter.c
    src/monitoring_output.c
    src/monitoring_remote.c
    src/monitoring_capture.c
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
)

target_compile_options(
    monitoring_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
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

        inc/qt/qt_application.h
        inc/qt/qt_main_window.h
        inc/qt/qt_process_model.h
        inc/qt/qt_monitor_controller.h
        inc/qt/qt_settings_dialog.h
        inc/qt/qt_theme.h
        inc/qt/qt_remote_dialog.h
        inc/qt/qt_wave_widget.h
        inc/qt/qt_graphs_window.h

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
