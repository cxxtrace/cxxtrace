cmake_minimum_required(VERSION 3.10)

include(cxxtrace_cotire)

add_executable(
  test_processor_id_manual
  ../event.cpp
  dtrace.cpp
  gnuplot.cpp
  main.cpp
  sample_checker.cpp
)
target_include_directories(test_processor_id_manual PRIVATE ..)
target_link_libraries(test_processor_id_manual PRIVATE cxxtrace dtrace gtest)
set_target_properties(test_processor_id_manual PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(test_processor_id_manual PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ../pch.h)
cxxtrace_cotire(test_processor_id_manual)
