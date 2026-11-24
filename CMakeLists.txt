cmake_minimum_required(VERSION 4.2)
project(apl1 C)

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

add_compile_options(-Wall -Wextra -pedantic -Werror -pedantic-errors)

#add_compile_definitions(_POSIX_C_SOURCE=200809L _XOPEN_SOURCE _GNU_SOURCE)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

include(LTO)
include(Sanitizers)
include(NotcursesDependency)
include(SQLiteDependency)
include(GNUInstallDirs)
include(CTest)

if(BUILD_TESTING)
    include(CriterionDependency)

    add_executable(apl1_tests
        tests/test_main.c
        tests/unit/test_error.c
        tests/integration/test_support.c
        tests/integration/test_db_connection.c
        tests/integration/test_db_path.c
        tests/integration/test_db_foreign_keys.c
        tests/integration/test_db_migrations.c
        tests/integration/test_db_initial_migration.c
        tests/integration/test_db_migration_rollback.c
    )

    target_include_directories(apl1_tests PRIVATE src tests/integration)

    target_link_libraries(apl1_tests PRIVATE
        apl_core
        apl_db
        Criterion::Criterion
        SQLite::SQLite3
    )

    add_test(
        NAME apl1_unit_tests
        COMMAND apl1_tests
    )

    add_executable(apl1_tui_tests
        tests/tui/test_tui_lifecycle.c
        tests/tui/test_tui_shell.c
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
endif()

add_library(apl_core STATIC
    src/core/error.c
)

target_include_directories(apl_core PUBLIC src)

add_library(apl_db STATIC
    src/db/connection.c
    src/db/path.c
    src/db/migrations.c
    src/db/migrations_list.c
)

target_include_directories(apl_db PUBLIC src tests/integration)
target_link_libraries(apl_db PUBLIC
    apl_core
    SQLite::SQLite3
)

add_library(apl_tui STATIC
    src/tui/lifecycle.c
    src/tui/shell.c
)

target_include_directories(apl_tui PUBLIC src)
target_link_libraries(apl_tui PUBLIC
    apl_core
    PRIVATE Notcurses::Notcurses
)

add_executable(apl1
    src/main.c
)

target_link_libraries(apl1 PRIVATE
    apl_core
    apl_db
    apl_tui
    Notcurses::Notcurses
    SQLite::SQLite3
)

install(TARGETS apl1)
