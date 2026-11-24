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
    )

    target_include_directories(apl1_tests PRIVATE src)

    target_link_libraries(apl1_tests PRIVATE
        Criterion::Criterion
        SQLite::SQLite3
    )

    add_test(
        NAME apl1_unit_tests
        COMMAND apl1_tests
    )
endif()

add_executable(apl1
    src/main.c
)

target_link_libraries(apl1 PRIVATE
    Notcurses::Notcurses
    SQLite::SQLite3
)

install(TARGETS apl1)
