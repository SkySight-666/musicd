cmake_minimum_required(VERSION 3.16)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED ENV{CROSS_TOOLCHAIN_PREFIX} AND NOT "$ENV{CROSS_TOOLCHAIN_PREFIX}" STREQUAL "")
  set(CMAKE_C_COMPILER "$ENV{CROSS_TOOLCHAIN_PREFIX}gcc")
  set(CMAKE_CXX_COMPILER "$ENV{CROSS_TOOLCHAIN_PREFIX}g++")

  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-sysroot
    OUTPUT_VARIABLE GCC_NATIVE_SYSROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set(CMAKE_SYSTEM_NAME Linux)
  set(CMAKE_SYSTEM_PROCESSOR arm)
  set(CMAKE_SYSROOT "${GCC_NATIVE_SYSROOT}")

  if(NOT "${CMAKE_SYSROOT}" STREQUAL "")
    set(CMAKE_SYSROOT_FLAG "--sysroot=${CMAKE_SYSROOT}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_SYSROOT_FLAG}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_SYSROOT_FLAG}")
  endif()

  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

  execute_process(COMMAND echo "musicd cross compiler: ${CMAKE_CXX_COMPILER}")
  execute_process(COMMAND echo "musicd sysroot: ${CMAKE_SYSROOT}")
endif()

project(musicd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(musicd
  src/main.cpp
  src/audio_output_manager.cpp
  src/playback_engine.cpp
  src/music_daemon.cpp
  src/ipc_server.cpp
  src/queue_manager.cpp
)

add_executable(musicctl
  src/musicctl.cpp
)

target_include_directories(musicd
  PRIVATE
    include
)

target_include_directories(musicctl
  PRIVATE
    include
)

target_compile_options(musicd PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)

target_compile_options(musicctl PRIVATE
  -Wall
  -Wextra
  -Wpedantic
)
