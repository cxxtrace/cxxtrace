cmake_minimum_required(VERSION 3.10)

include(cotire)

function (CXXTRACE_COTIRE)
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
