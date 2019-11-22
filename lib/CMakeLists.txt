cmake_minimum_required(VERSION 3.10)

find_package(Librseq)
find_package(Threads REQUIRED)

add_library(
  cxxtrace
  add.cpp
  chrome_trace_event_format.cpp
  clock.cpp
  clock_extra.cpp
  file_descriptor.cpp
  iostream.cpp
  processor.cpp
  real_synchronization.cpp
  rseq.cpp
  snapshot.cpp
  thread.cpp
)
target_include_directories(cxxtrace PUBLIC include PRIVATE)
target_link_libraries(cxxtrace PRIVATE Threads::Threads)

if (TARGET Librseq::librseq)
  target_link_libraries(cxxtrace PRIVATE Librseq::librseq)
endif ()
