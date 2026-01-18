cmake_minimum_required(VERSION 3.15)
project(distance_video C)

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



# Main executable
add_executable(distance_video src/main.c)

target_link_libraries(distance_video
    PRIVATE
    ${TURBOJPEG_LIBRARY}
)

if(WIN32)
    target_link_libraries(distance_video PRIVATE gdi32 user32)
endif()

# Include directories
target_include_directories(distance_video
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${TURBOJPEG_INCLUDE_DIR}
)
