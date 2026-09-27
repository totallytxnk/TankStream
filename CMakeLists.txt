cmake_minimum_required(VERSION 3.16)
project(TankStream VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(TANKSTREAM_BUILD_CONSOLE "Build console test harness" ON)
option(TANKSTREAM_BUILD_UI "Build Qt 6 desktop UI" ON)

# ------------------------------------------------------------
# Local libdatachannel (no network at configure time)
# ------------------------------------------------------------
set(TANKSTREAM_LIBDATACHANNEL_ROOT "C:/Users/PC/libdatachannel" CACHE PATH
    "Path to local libdatachannel source tree (with submodules)")

if(NOT EXISTS "${TANKSTREAM_LIBDATACHANNEL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "libdatachannel not found at:\n  ${TANKSTREAM_LIBDATACHANNEL_ROOT}\n\n"
        "On this machine, clone once:\n"
        "  git clone --recursive --branch v0.24.5 --depth 1 "
        "https://github.com/paullouisageneau/libdatachannel.git\n"
        "Then re-run cmake with:\n"
        "  -DTANKSTREAM_LIBDATACHANNEL_ROOT=/path/to/libdatachannel")
endif()

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(FFMPEG REQUIRED
    libavcodec libavformat libavutil libswscale libavdevice)

if(TANKSTREAM_BUILD_UI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
endif()

# ------------------------------------------------------------
# libdatachannel from local tree
# ------------------------------------------------------------
set(USE_SYSTEM_SRTP ON CACHE BOOL "" FORCE)
set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(USE_MBEDTLS OFF CACHE BOOL "" FORCE)

add_subdirectory("${TANKSTREAM_LIBDATACHANNEL_ROOT}"
                 "${CMAKE_BINARY_DIR}/libdatachannel-build" EXCLUDE_FROM_ALL)
set(DATACHANNEL_TARGET datachannel)

# ------------------------------------------------------------
# Core library
# ------------------------------------------------------------
add_library(tankstream_core STATIC
    src/H264Encoder.cpp
    src/H264Decoder.cpp
    src/WebRTCBridge.cpp
    src/WebRTCReceiver.cpp
)

add_library(TankStream::Core ALIAS tankstream_core)

target_include_directories(tankstream_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(tankstream_core
    PUBLIC
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        ${DATACHANNEL_TARGET}
    PRIVATE
        ${FFMPEG_LIBRARIES}
)

target_compile_options(tankstream_core PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wno-unused-parameter>
)

if(FFMPEG_LIBRARY_DIRS)
    target_link_directories(tankstream_core PUBLIC ${FFMPEG_LIBRARY_DIRS})
endif()

# ------------------------------------------------------------
# Console harness
# ------------------------------------------------------------
if(TANKSTREAM_BUILD_CONSOLE)
    add_executable(tankstream src/main.cpp)
    target_link_libraries(tankstream PRIVATE TankStream::Core)
    if(WIN32)
        set_target_properties(tankstream PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()
endif()

# ------------------------------------------------------------
# Qt 6 UI (+ LAN discovery)
# ------------------------------------------------------------
if(TANKSTREAM_BUILD_UI)
    add_executable(tankstream-ui
        src/ui/main_ui.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.hpp
        src/ui/LanDiscovery.cpp
        src/ui/LanDiscovery.hpp
    )
    target_include_directories(tankstream-ui PRIVATE src/ui)
    target_link_libraries(tankstream-ui PRIVATE
        TankStream::Core
        Qt6::Widgets
        Qt6::Network
    )
    if(WIN32)
        set_target_properties(tankstream-ui PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            WIN32_EXECUTABLE FALSE
        )
    endif()
endif()

message(STATUS "========================================")
message(STATUS " TankStream ${PROJECT_VERSION}")
message(STATUS "  libdatachannel : ${TANKSTREAM_LIBDATACHANNEL_ROOT}")
message(STATUS "  Console app    : ${TANKSTREAM_BUILD_CONSOLE}")
message(STATUS "  Qt UI + LAN    : ${TANKSTREAM_BUILD_UI}")
message(STATUS "========================================")
