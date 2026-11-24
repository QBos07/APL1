include(FetchContent)
find_package(PkgConfig QUIET)

# Prefer the system installation. Check the full `notcurses` module first;
# some distributions ship notcurses_init only in the full library.
if(PkgConfig_FOUND)
    pkg_check_modules(NOTCURSES QUIET IMPORTED_TARGET notcurses)
endif()

if(NOT NOTCURSES_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(NOTCURSES QUIET IMPORTED_TARGET notcurses-core)
endif()

if(NOTCURSES_FOUND)
    message(STATUS "Using system Notcurses: ${NOTCURSES_VERSION}")

    add_library(Notcurses::Notcurses ALIAS PkgConfig::NOTCURSES)
else()
    message(STATUS "System Notcurses not found; fetching upstream")

    # Disable unnecessary upstream components.
    set(BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(USE_POC OFF CACHE BOOL "" FORCE)
    set(USE_PANDOC OFF CACHE BOOL "" FORCE)
    set(USE_DOXYGEN OFF CACHE BOOL "" FORCE)
    set(USE_CXX OFF CACHE BOOL "" FORCE)
    set(USE_MULTIMEDIA none CACHE STRING "" FORCE)

    FetchContent_Declare(
            notcurses
            GIT_REPOSITORY https://github.com/dankamongmen/notcurses.git
            GIT_TAG        v3.0.17
            GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(notcurses)

    add_library(Notcurses::Notcurses ALIAS notcurses-core)
endif()
