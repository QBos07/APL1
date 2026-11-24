cmake_minimum_required(VERSION 4.2)
project(apl1 C)

add_compile_options(-Wall -Wextra -pedantic -Werror -pedantic-errors)

add_compile_definitions(_POSIX_C_SOURCE=200809L _XOPEN_SOURCE _GNU_SOURCE)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)

if( lto_supported )
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

set(COMMON_FILES input.c dyn_array.c)

add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
add_link_options(-fsanitize=address,undefined)

add_executable(apl1 main.c)
