cmake_minimum_required(VERSION 3.16)
project(TankStream VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
option(TANKSTREAM_USE_SYSTEM_LIBDATACHANNEL
       "Use system-installed libdatachannel instead of FetchContent"
       OFF)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# FFmpeg (required)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
    libavdevice
)

# ------------------------------------------------------------
# libdatachannel
# ------------------------------------------------------------
if(TANKSTREAM_USE_SYSTEM_LIBDATACHANNEL)
    find_package(LibDataChannel REQUIRED)
    set(DATACHANNEL_TARGET LibDataChannel::LibDataChannel)
else()
    include(FetchContent)

    # Prefer system libsrtp to avoid MinGW -Werror format issues in the submodule
    set(USE_SYSTEM_SRTP ON CACHE BOOL "" FORCE)
    set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
    set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
    set(NO_TESTS ON CACHE BOOL "" FORCE)
    set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
    set(USE_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(PREFER_SYSTEM_LIB OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        libdatachannel
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG        v0.24.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(libdatachannel)
    set(DATACHANNEL_TARGET datachannel)
endif()

# ------------------------------------------------------------
# TankStream executable
# ------------------------------------------------------------
add_executable(tankstream
    src/main.cpp
    src/H264Encoder.cpp
    src/WebRTCBridge.cpp
)

target_include_directories(tankstream PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(tankstream PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${FFMPEG_LIBRARIES}
    ${DATACHANNEL_TARGET}
)

target_compile_options(tankstream PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Ensure library search paths are correct (important on MSYS2)
if(FFMPEG_LIBRARY_DIRS)
    target_link_directories(tankstream PRIVATE ${FFMPEG_LIBRARY_DIRS})
endif()

# On Windows / MinGW, make sure we can find the DLLs at runtime when running from build/
if(WIN32)
    set_target_properties(tankstream PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

message(STATUS "========================================")
message(STATUS " TankStream configured")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  FFmpeg       : ${FFMPEG_LIBRARIES}")
message(STATUS "  OpenSSL      : ${OPENSSL_VERSION}")
message(STATUS "========================================")
