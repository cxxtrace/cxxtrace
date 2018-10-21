cmake_minimum_required(VERSION 3.10)
project(cxxtrace)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_subdirectory(vendor)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(test)
endif ()
