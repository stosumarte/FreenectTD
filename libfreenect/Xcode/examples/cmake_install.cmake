# Install script for directory: /Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/examples

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
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-camtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-camtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-camtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-camtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-camtest")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-camtest.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-camtest.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-camtest.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-camtest.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-wavrecord")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-wavrecord")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-wavrecord")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-wavrecord")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-wavrecord")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-wavrecord.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-wavrecord.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-wavrecord.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-wavrecord.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-glview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-glview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-glview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-glview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-regview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-regview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-regview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-regview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-hiview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-hiview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-hiview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-hiview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-hiview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-hiview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-hiview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-hiview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-hiview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-chunkview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-chunkview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-chunkview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-chunkview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-chunkview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-chunkview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-chunkview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-chunkview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-chunkview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-micview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-micview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-micview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-micview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-micview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-micview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-micview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-micview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-micview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-regtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-regtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-regtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-regtest")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-regtest")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regtest.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regtest.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regtest.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-regtest.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-tiltdemo")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-tiltdemo")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-tiltdemo")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-tiltdemo")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-tiltdemo")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-tiltdemo.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-tiltdemo.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-tiltdemo.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-tiltdemo.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Debug/freenect-glpclview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Debug"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/Release/freenect-glpclview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/Release"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/MinSizeRel/freenect-glpclview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/MinSizeRel"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      endif()
    endif()
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/bin/RelWithDebInfo/freenect-glpclview")
    if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview" AND
       NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      execute_process(COMMAND /usr/bin/install_name_tool
        -delete_rpath "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/lib/RelWithDebInfo"
        "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/strip" -u -r "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/freenect-glpclview")
      endif()
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glpclview.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glpclview.dir/install-cxx-module-bmi-Release.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glpclview.dir/install-cxx-module-bmi-MinSizeRel.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    include("/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/CMakeFiles/freenect-glpclview.dir/install-cxx-module-bmi-RelWithDebInfo.cmake" OPTIONAL)
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/marte/vjing/FreenectTD/FreenectTD/libfreenect/Xcode/examples/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
