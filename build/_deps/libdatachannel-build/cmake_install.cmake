# Install script for directory: C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/TankStream")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/msys64/ucrt64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/libdatachannel.dll.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/libdatachannel.dll")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libdatachannel.dll" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libdatachannel.dll")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "C:/msys64/ucrt64/bin/strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/libdatachannel.dll")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/rtc" TYPE FILE FILES
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/candidate.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/channel.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/configuration.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/datachannel.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/dependencydescriptor.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/description.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/iceudpmuxlistener.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/mediahandler.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtcpreceivingsession.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/common.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/global.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/message.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/frameinfo.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/peerconnection.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/reliability.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtc.h"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtc.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtp.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/track.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/websocket.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/websocketserver.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtppacketizationconfig.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtcpsrreporter.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtppacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtpdepacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/h264rtppacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/h264rtpdepacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/nalunit.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/h265rtppacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/h265rtpdepacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/h265nalunit.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/av1rtppacketizer.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rtcpnackresponder.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/utils.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/plihandler.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/pacinghandler.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/rembhandler.hpp"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-src/include/rtc/version.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake"
         "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel/LibDataChannelTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/CMakeFiles/Export/32c821eb1e7b36c3a3818aec162f7fd2/LibDataChannelTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/LibDataChannel" TYPE FILE FILES
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/LibDataChannelConfig.cmake"
    "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/LibDataChannelConfigVersion.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/PC/Downloads/tankstream(1)/tankstream/build/_deps/libdatachannel-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
