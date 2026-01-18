cmake_minimum_required(VERSION 3.15)
project(distance_encoder C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find TurboJPEG
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(TURBOJPEG libturbojpeg)
endif()

if(NOT TURBOJPEG_FOUND)
    find_library(TURBOJPEG_LIBRARY turbojpeg)
    find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h)
    
    if(TURBOJPEG_LIBRARY AND TURBOJPEG_INCLUDE_DIR)
        set(TURBOJPEG_FOUND TRUE)
    else()
        message(FATAL_ERROR "TurboJPEG not found. Install with: pacman -S mingw-w64-ucrt-x86_64-libjpeg-turbo")
    endif()
endif()

# cJSON is header-only, add to include path
set(CJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/cJSON")
if(NOT EXISTS "${CJSON_DIR}")
    message(FATAL_ERROR "cJSON not found. Download from https://github.com/DaveGamble/cJSON")
endif()

# Main executable
add_executable(distance_encoder
    src/main.c
    src/config.c
    src/shared_memory.c
    src/capture.c
    src/capture/gdi.c
)

# Include directories
target_include_directories(distance_encoder
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
    ${TURBOJPEG_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(distance_encoder
    PRIVATE
    ${TURBOJPEG_LIBRARY}
    gdi32
    user32
)
