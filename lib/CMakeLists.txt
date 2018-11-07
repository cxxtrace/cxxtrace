cmake_minimum_required(VERSION 3.10)

add_library(
  cxxtrace
  add.cpp
  snapshot.cpp
  thread.cpp
  unbounded_storage.cpp
  unbounded_unsafe_storage.cpp
)
target_include_directories(cxxtrace PUBLIC include PRIVATE private_include)
