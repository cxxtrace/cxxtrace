cmake_minimum_required(VERSION 3.10)
project(cxxtrace)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cotire/CMake")

set(
  CXXTRACE_CDSCHECKER
  TRUE
  CACHE
  BOOL
  "Enable extra testing using CDSChecker."
)
set(
  CXXTRACE_FILECHECK
  ""
  CACHE
  FILEPATH
  "Path to the LLVM FileCheck program for compile error tests"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(vendor)

add_subdirectory(example)
add_subdirectory(lib)
include(CTest)
if (BUILD_TESTING)
  add_subdirectory(test)
endif ()
add_subdirectory(tools)
