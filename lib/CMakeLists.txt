cmake_minimum_required(VERSION 3.10)

add_library(cxxtrace add.cpp clock.cpp clock_extra.cpp snapshot.cpp thread.cpp)
target_include_directories(cxxtrace PUBLIC include PRIVATE private_include)
