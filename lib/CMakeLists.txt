cmake_minimum_required(VERSION 3.10)

add_library(cxxtrace config.cpp span.cpp snapshot.cpp storage.cpp thread.cpp)
target_include_directories(cxxtrace PUBLIC include PRIVATE private_include)
