cmake_minimum_required(VERSION 3.10)

add_library(cxxtrace sample.cpp span.cpp snapshot.cpp testing.cpp)
target_include_directories(cxxtrace PUBLIC include PRIVATE private_include)
