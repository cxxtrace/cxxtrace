cmake_minimum_required(VERSION 3.10)

include(cotire)

function (CXXTRACE_COTIRE TARGET)
  # HACK(strager): On Linux, in the following scenario, pre-compiled header
  # generation is buggy:
  #
  # * Target A depends on target B.
  # * Target B depends on Threads::Threads.
  # * cxxtrace_cotire is called on target A.
  #
  # In this case, cotire pre-compiles headers for target A without -D_REENTRANT,
  # but the build system compiles target A's sources with -D_REENTRANT. GCC
  # warns about this discrepancy, and Clang reports a fatal error.
  #
  # As a workaround, make all targets explicitly depend on Threads::Threads
  # (thus both cotire and the build system see -D_REENTRANT).
  find_package(Threads)
  if (TARGET Threads::Threads)
    target_link_libraries(${TARGET} PRIVATE Threads::Threads)
  endif ()

  # HACK(strager): If CMAKE_INCLUDE_SYSTEM_FLAG_CXX is "-isystem ", cotire
  # invokes the compiler with "-isystem /path/to/include" as one argument. This
  # confuses Clang. Make cotire invoke the compiler with "-isystem" and
  # "/path/to/include" as separate arguments.
  # TODO(strager): Report this bug upstream.
  set(OLD_CMAKE_INCLUDE_SYSTEM_FLAG_CXX "${CMAKE_INCLUDE_SYSTEM_FLAG_CXX}")
  string(
    REPLACE
    " "
    ";"
    CMAKE_INCLUDE_SYSTEM_FLAG_CXX
    "${CMAKE_INCLUDE_SYSTEM_FLAG_CXX}")
  cotire(${ARGV})
  set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "${OLD_CMAKE_INCLUDE_SYSTEM_FLAG_CXX}")
endfunction ()
