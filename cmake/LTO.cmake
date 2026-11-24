include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)

if( lto_supported )
    message(STATUS "IPO / LTO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()
