cmake_minimum_required(VERSION 4.2)
project(apl1 VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -pedantic-errors
    -Wcast-qual
    -Wformat-security
    -Wmissing-declarations
    -Wmissing-prototypes
    -Wold-style-definition
    -Wpointer-arith
    -Wshadow
    -Wstrict-prototypes
    -Wundef
    -Wvla
)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

include(LTO)
include(Sanitizers)
include(NotcursesDependency)
include(SQLiteDependency)
include(GNUInstallDirs)
include(CTest)

option(APL1_ENABLE_TUI_SMOKE
        "Register the interactive TUI smoke test (requires Python 3, Xvfb, xterm, and xdotool)"
        OFF
)

if(APL1_ENABLE_TUI_SMOKE AND NOT BUILD_TESTING)
    message(FATAL_ERROR
            "APL1_ENABLE_TUI_SMOKE requires BUILD_TESTING=ON so the smoke test can be registered"
    )
endif()

if(BUILD_TESTING)
    include(CriterionDependency)

    add_library(apl_test_support STATIC
        tests/support/db_fixture.c
        tests/support/test_support.c
    )

    target_include_directories(apl_test_support
        PUBLIC tests/support
        PRIVATE src
    )

    target_link_libraries(apl_test_support PRIVATE
        apl_core
        apl_db
        SQLite3::SQLite3
    )

    add_executable(apl1_tests
        tests/unit/test_error.c
        tests/unit/test_exit_code.c
        tests/unit/test_args.c
        tests/unit/test_reference.c
        tests/unit/test_team.c
        tests/unit/test_text.c
        tests/integration/test_test_support.c
        tests/integration/test_activity_repository.c
        tests/integration/test_car_repository.c
        tests/integration/test_db_connection.c
        tests/integration/test_db_path.c
        tests/integration/test_db_foreign_keys.c
        tests/integration/test_db_migrations.c
        tests/integration/test_db_initial_migration.c
        tests/integration/test_db_migration_rollback.c
        tests/integration/test_driver_repository.c
        tests/integration/test_membership_repository.c
        tests/integration/test_reference_services.c
        tests/integration/test_season_repository.c
        tests/integration/test_app_lifecycle.c
        tests/integration/test_cli.c
        tests/integration/test_settings_repository.c
        tests/integration/test_team_audit.c
        tests/integration/test_team_repository.c
        tests/integration/test_team_isolation.c
        tests/integration/test_team_service.c
        tests/integration/test_transaction.c
    )

    target_include_directories(apl1_tests PRIVATE src)

    target_link_libraries(apl1_tests PRIVATE
        apl_core
        apl_domain
        apl_db
        apl_app
        apl_test_support
        Criterion::Criterion
        SQLite3::SQLite3
    )

    add_test(
        NAME apl1_unit_tests
        COMMAND apl1_tests
    )

    add_executable(apl1_tui_tests
        tests/tui/test_tui_action.c
        tests/tui/test_tui_command.c
        tests/tui/test_tui_command_line.c
        tests/tui/test_tui_dialog.c
        tests/tui/test_tui_focus.c
        tests/tui/test_tui_frame.c
        tests/tui/test_tui_lifecycle.c
        tests/tui/test_tui_layout.c
        tests/tui/test_tui_list.c
        tests/tui/test_tui_menu.c
        tests/tui/test_tui_navigation.c
        tests/tui/test_tui_shell.c
        tests/tui/test_tui_status.c
        tests/tui/test_tui_text.c
        tests/tui/test_tui_theme.c
    )

    target_include_directories(apl1_tui_tests PRIVATE src)

    target_link_libraries(apl1_tui_tests PRIVATE
        apl_tui
        Criterion::Criterion
    )

    add_test(
        NAME apl1_tui_tests
        COMMAND apl1_tui_tests
    )

    if(APL1_ENABLE_TUI_SMOKE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        find_program(APL1_XVFB_EXECUTABLE NAMES Xvfb REQUIRED)
        find_program(APL1_XTERM_EXECUTABLE NAMES xterm REQUIRED)
        find_program(APL1_XDOTOOL_EXECUTABLE NAMES xdotool REQUIRED)

        add_test(
            NAME apl1_tui_smoke
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/smoke_tui.py
                --app $<TARGET_FILE:apl1>
                --timeout 20
                --output ${CMAKE_CURRENT_BINARY_DIR}/apl1-tui-smoke.log
        )

        set_tests_properties(apl1_tui_smoke PROPERTIES
            TIMEOUT 90
            LABELS "tui;smoke"
        )
    endif()
endif()

add_library(apl_core STATIC
    src/core/error.c
    src/core/text.c
)

target_include_directories(apl_core PUBLIC src)

add_library(apl_domain STATIC
    src/domain/reference.c
    src/domain/team.c
)

target_include_directories(apl_domain PUBLIC src)
target_link_libraries(apl_domain PUBLIC
    apl_core
)

add_library(apl_db STATIC
    src/db/activity_repository.c
    src/db/car_repository.c
    src/db/connection.c
    src/db/driver_repository.c
    src/db/membership_repository.c
    src/db/migrations.c
    src/db/migrations_list.c
    src/db/path.c
    src/db/season_repository.c
    src/db/settings_repository.c
    src/db/sqlite_util.c
    src/db/team_repository.c
    src/db/transaction.c
)

target_include_directories(apl_db PUBLIC src)
target_link_libraries(apl_db PUBLIC
    apl_core
    apl_domain
    SQLite3::SQLite3
)

add_library(apl_tui STATIC
    src/tui/action.c
    src/tui/command.c
    src/tui/command_line.c
    src/tui/dialog.c
    src/tui/focus.c
    src/tui/frame.c
    src/tui/layout.c
    src/tui/lifecycle.c
    src/tui/list.c
    src/tui/menu.c
    src/tui/navigation.c
    src/tui/shell.c
    src/tui/status.c
    src/tui/text.c
    src/tui/theme.c
)

target_include_directories(apl_tui PUBLIC src)
target_link_libraries(apl_tui PUBLIC
    apl_core
    apl_app
    PRIVATE Notcurses::Notcurses
)

add_library(apl_app STATIC
    src/app/activity_service.c
    src/app/app.c
    src/app/audit.c
    src/app/args.c
    src/app/car_service.c
    src/app/driver_service.c
    src/app/exit_code.c
    src/app/membership_service.c
    src/app/season_service.c
    src/app/team_service.c
    src/app/version.c
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(APL1_BUILD_TYPE "unspecified")
elseif(CMAKE_BUILD_TYPE)
    set(APL1_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(APL1_BUILD_TYPE "$<CONFIG>")
endif()

target_compile_definitions(apl_app PUBLIC
    APL1_VERSION_STRING="${PROJECT_VERSION}"
    APL1_BUILD_TYPE="${APL1_BUILD_TYPE}"
)

target_include_directories(apl_app PUBLIC src)
target_link_libraries(apl_app PUBLIC
    apl_core
    apl_domain
    apl_db
)

add_executable(apl1
    src/main.c
)

if(BUILD_TESTING)
    target_compile_definitions(apl1_tests PRIVATE
        APL1_BINARY_PATH="$<TARGET_FILE:apl1>"
    )
    add_dependencies(apl1_tests apl1)
endif()

target_link_libraries(apl1 PRIVATE
    apl_app
    apl_tui
    Notcurses::Notcurses
)

install(TARGETS apl1
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
