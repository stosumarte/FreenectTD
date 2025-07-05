# Install script for directory: /Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/wrappers/cpp

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
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
  set(CMAKE_OBJDUMP "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/objdump")
endif()

set(CMAKE_BINARY_DIR "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode")

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libfreenect" TYPE FILE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/wrappers/cpp/libfreenect.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-cppview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-cppview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-cppview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-cppview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cppview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cppview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cppview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cppview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cppview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-cpp_pcview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-cpp_pcview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-cpp_pcview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-cpp_pcview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-cpp_pcview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cpp_pcview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cpp_pcview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cpp_pcview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/CMakeFiles/freenect-cpp_pcview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/wrappers/cpp/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
