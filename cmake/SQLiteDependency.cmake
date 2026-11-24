
include_guard(GLOBAL)

include(FetchContent)

option(USE_BUNDLED_SQLITE
        "Build SQLite from upstream sources instead of using the system installation"
        OFF
)

# --------------------------------------------------
# System installation
# --------------------------------------------------

if(NOT USE_BUNDLED_SQLITE)
    find_package(SQLite3 QUIET)
endif()

if(SQLite3_FOUND AND NOT USE_BUNDLED_SQLITE)

    message(STATUS "Using system SQLite: ${SQLite3_VERSION}")

else()

    message(STATUS "Fetching upstream SQLite amalgamation")

    # --------------------------------------------------
    # Fetch upstream source
    # --------------------------------------------------

    FetchContent_Declare(
            sqlite3_amalgamation
            URL
            https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
            URL_HASH
            SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    )

    # Download and extract. The archive has no CMakeLists.txt,
    # so FetchContent_MakeAvailable will not add it as a subproject.
    FetchContent_MakeAvailable(sqlite3_amalgamation)

    # --------------------------------------------------
    # Locate amalgamation source
    # --------------------------------------------------

    file(GLOB _sqlite3_sources
            "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c"
            "${sqlite3_amalgamation_SOURCE_DIR}/sqlite-amalgamation-*/sqlite3.c"
    )

    list(LENGTH _sqlite3_sources _sqlite3_source_count)

    if(NOT _sqlite3_source_count EQUAL 1)
        message(FATAL_ERROR
                "Could not uniquely locate sqlite3.c in "
                "${sqlite3_amalgamation_SOURCE_DIR}"
        )
    endif()

    list(GET _sqlite3_sources 0 _sqlite3_source)

    get_filename_component(
            _sqlite3_include_dir
            "${_sqlite3_source}"
            DIRECTORY
    )

    # --------------------------------------------------
    # Build SQLite
    # --------------------------------------------------

    find_package(Threads REQUIRED)

    add_library(sqlite3_vendor STATIC
            "${_sqlite3_source}"
    )

    target_include_directories(sqlite3_vendor PUBLIC
            "${_sqlite3_include_dir}"
    )

    target_link_libraries(sqlite3_vendor PUBLIC
            Threads::Threads
            ${CMAKE_DL_LIBS}
    )

    set_target_properties(sqlite3_vendor PROPERTIES
            POSITION_INDEPENDENT_CODE ON
    )

    # --------------------------------------------------
    # Common target
    # --------------------------------------------------

    add_library(SQLite::SQLite3 ALIAS sqlite3_vendor)

endif()
