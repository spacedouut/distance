cmake_minimum_required(VERSION 3.15)

# OBJCXX is needed for the macOS backend (.mm files).
# It's declared unconditionally so CMake sets up the language early;
# the .mm source is only added on Apple targets below.
project(distance_encoder C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# TurboJPEG — required on all platforms
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    if(APPLE)
        # Homebrew on Intel Mac puts pkg-config files here; add it so
        # pkg_check_modules can find libturbojpeg without a PATH export.
        list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/jpeg-turbo)
        set(ENV{PKG_CONFIG_PATH}
            "/usr/local/opt/jpeg-turbo/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
    pkg_check_modules(TURBOJPEG libturbojpeg)
endif()

if(NOT TURBOJPEG_FOUND)
    # Fallback: look for the library and header directly
    find_library(TURBOJPEG_LIBRARY NAMES turbojpeg
        HINTS /usr/local/opt/jpeg-turbo/lib /usr/local/lib)
    find_path(TURBOJPEG_INCLUDE_DIR turbojpeg.h
        HINTS /usr/local/opt/jpeg-turbo/include /usr/local/include)

    if(TURBOJPEG_LIBRARY AND TURBOJPEG_INCLUDE_DIR)
        set(TURBOJPEG_FOUND TRUE)
        set(TURBOJPEG_LIBRARIES ${TURBOJPEG_LIBRARY})
        set(TURBOJPEG_INCLUDE_DIRS ${TURBOJPEG_INCLUDE_DIR})
    else()
        message(FATAL_ERROR
            "TurboJPEG not found.\n"
            "  macOS:   brew install jpeg-turbo\n"
            "  Windows: pacman -S mingw-w64-ucrt-x86_64-libjpeg-turbo")
    endif()
endif()

# ---------------------------------------------------------------------------
# cJSON — bundled under src/cJSON/
# ---------------------------------------------------------------------------
set(CJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cJSON")
if(NOT EXISTS "${CJSON_DIR}/cJSON.h")
    message(FATAL_ERROR
        "cJSON not found at ${CJSON_DIR}.\n"
        "Run: curl -fsSL https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h "
        "-o ${CJSON_DIR}/cJSON.h && curl -fsSL "
        "https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c "
        "-o ${CJSON_DIR}/cJSON.c")
endif()

# ---------------------------------------------------------------------------
# Common sources (all platforms)
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/shared_memory.cpp
    src/capture.cpp
    src/cJSON/cJSON.c
)

# ---------------------------------------------------------------------------
# Platform-specific sources
# ---------------------------------------------------------------------------
if(APPLE)
    list(APPEND SOURCES src/capture/macos.mm)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.3" CACHE STRING "")
elseif(WIN32)
    list(APPEND SOURCES
        src/capture/gdi.cpp
        src/capture/dxgi.cpp
    )
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(distance_encoder ${SOURCES})

target_include_directories(distance_encoder
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TURBOJPEG_INCLUDE_DIRS}
)

target_link_libraries(distance_encoder
    PRIVATE
    ${TURBOJPEG_LIBRARIES}
)

# ---------------------------------------------------------------------------
# Platform-specific link libraries
# ---------------------------------------------------------------------------
if(APPLE)
    target_link_libraries(distance_encoder
        PRIVATE
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework CoreGraphics"
        "-framework Foundation"
        "-framework AppKit"
    )
elseif(WIN32)
    target_link_libraries(distance_encoder
        PRIVATE
        d3d11
        dxgi
        gdi32
        user32
    )
endif()
