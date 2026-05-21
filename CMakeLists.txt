cmake_minimum_required(VERSION 3.10)
project(qrreader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_library(qrreader_win_common STATIC qrreader_win_common.cpp)
  target_include_directories(qrreader_win_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(qrreader_win_common PUBLIC _WIN32_WINNT=0x0601 WINVER=0x0601)
  target_link_libraries(qrreader_win_common PUBLIC
    advapi32
    shell32
    user32
    userenv
    wtsapi32)

  macro(qrreader_apply_msvc_runtime target)
    if(MSVC)
      target_compile_options(${target} PRIVATE /utf-8 /W4)
      if(QRREADER_STATIC_MSVC_RUNTIME)
        target_compile_options(${target} PRIVATE
          $<$<CONFIG:Debug>:/MTd>
          $<$<CONFIG:Release>:/MT>
          $<$<CONFIG:RelWithDebInfo>:/MT>
          $<$<CONFIG:MinSizeRel>:/MT>)
      endif()
    endif()
  endmacro()

  option(QRREADER_STATIC_MSVC_RUNTIME "Link MSVC runtime statically (/MT)" OFF)

  add_executable(qrreader_tray WIN32 qrreader_tray.cpp qrreader_tray.rc)
  target_link_libraries(qrreader_tray PRIVATE qrreader_win_common)
  qrreader_apply_msvc_runtime(qrreader_tray)

  add_executable(qrreader_service qrreader_service.cpp)
  target_link_libraries(qrreader_service PRIVATE qrreader_win_common)
  qrreader_apply_msvc_runtime(qrreader_service)

  add_executable(qrreader_windows qrreader_windows.cpp)
  target_link_libraries(qrreader_windows PRIVATE qrreader_win_common)
  qrreader_apply_msvc_runtime(qrreader_windows)
else()
  option(QRREADER_STATIC_LIBSTDCPP
    "Link libstdc++ and libgcc statically (recommended for distro portability)"
    OFF)
  option(QRREADER_FULLY_STATIC
    "Fully static link including libc (use a musl toolchain; not recommended with glibc)"
    OFF)

  add_executable(qrreader_linux qrreader_linux.cpp)

  find_package(Threads REQUIRED)
  target_link_libraries(qrreader_linux PRIVATE Threads::Threads)

  if(QRREADER_FULLY_STATIC AND QRREADER_STATIC_LIBSTDCPP)
    message(WARNING "QRREADER_FULLY_STATIC is ON; QRREADER_STATIC_LIBSTDCPP is redundant (using full -static).")
  endif()

  if(QRREADER_FULLY_STATIC)
    target_link_libraries(qrreader_linux PRIVATE -static)
  elseif(QRREADER_STATIC_LIBSTDCPP)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_link_libraries(qrreader_linux PRIVATE -static-libgcc -static-libstdc++)
    else()
      message(WARNING "QRREADER_STATIC_LIBSTDCPP: only GNU and Clang are supported; option ignored.")
    endif()
  endif()
endif()
