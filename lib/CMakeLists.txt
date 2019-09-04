cmake_minimum_required(VERSION 3.10)

add_library(
  cxxtrace
  add.cpp
  chrome_trace_event_format.cpp
  clock.cpp
  clock_extra.cpp
  file_descriptor.cpp
  processor.cpp
  snapshot.cpp
  thread.cpp
)
target_include_directories(cxxtrace PUBLIC include PRIVATE)
