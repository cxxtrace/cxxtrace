cmake_minimum_required(VERSION 3.10)

add_executable(cxxtrace_check_rseq main.cpp)
target_link_libraries(cxxtrace_check_rseq PRIVATE cxxtrace_check_rseq_library)

add_library(
  cxxtrace_check_rseq_library
  capstone.cpp
  elf_function.cpp
  libelf.cpp
  parse_binary.cpp
  rseq_analyzer.cpp
)
target_link_libraries(
  cxxtrace_check_rseq_library
  PUBLIC
  cxxtrace
  elf
  PRIVATE
  capstone
)
target_include_directories(cxxtrace_check_rseq_library PUBLIC .)
