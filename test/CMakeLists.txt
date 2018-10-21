cmake_minimum_required(VERSION 3.10)

add_executable(test_cxxtrace test_cxxtrace.cpp)
target_link_libraries(test_cxxtrace PRIVATE gtest gtest_main)
add_test(NAME test_cxxtrace COMMAND test_cxxtrace)
