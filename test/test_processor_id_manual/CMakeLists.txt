cmake_minimum_required(VERSION 3.10)

include(cxxtrace_cotire)

find_package(CxxtraceStdFilesystem REQUIRED)

if (APPLE)
  add_library(test_processor_id_manual_tracer STATIC dtrace.cpp)
  target_include_directories(test_processor_id_manual_tracer PRIVATE ..)
  target_link_libraries(test_processor_id_manual_tracer PRIVATE cxxtrace dtrace)
endif ()
if ("${CMAKE_SYSTEM_NAME}" STREQUAL Linux)
  add_library(
    test_processor_id_manual_tracer
    STATIC
    linux_perf_event.cpp
    mmap_mapping.cpp
  )
  target_include_directories(test_processor_id_manual_tracer PRIVATE ..)
  target_link_libraries(
    test_processor_id_manual_tracer
    PRIVATE
    cxxtrace
    cxxtrace_support
  )
endif ()
set_target_properties(test_processor_id_manual_tracer PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(test_processor_id_manual_tracer PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ../pch.h)
cxxtrace_cotire(test_processor_id_manual_tracer)

add_executable(
  test_processor_id_manual
  gnuplot.cpp
  main.cpp
  sample_checker.cpp
)
target_include_directories(test_processor_id_manual PRIVATE ..)
target_link_libraries(
  test_processor_id_manual
  PRIVATE
  CxxtraceStdFilesystem
  cxxtrace
  cxxtrace_support
  gtest
  test_processor_id_manual_tracer
)
set_target_properties(test_processor_id_manual PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(test_processor_id_manual PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ../pch.h)
cxxtrace_cotire(test_processor_id_manual)
