cmake_minimum_required(VERSION 3.10)

include(cxxtrace_cotire)

add_executable(
  cxxtrace_test_check_rseq
  assembler.cpp
  test_assembler.cpp
  test_elf_function.cpp
  test_rseq_analyzer.cpp
)
target_link_libraries(
  cxxtrace_test_check_rseq
  PRIVATE
  cxxtrace
  cxxtrace_check_rseq_library
  cxxtrace_support
  elf
  gmock
  gmock_main
  gtest
)
set_target_properties(cxxtrace_test_check_rseq PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(cxxtrace_test_check_rseq PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT ../pch.h)
cxxtrace_cotire(cxxtrace_test_check_rseq)
add_test(NAME cxxtrace_test_check_rseq COMMAND cxxtrace_test_check_rseq)
