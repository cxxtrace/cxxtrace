cmake_minimum_required(VERSION 3.10)

add_executable(test_cxxtrace test_span.cpp)
target_link_libraries(test_cxxtrace PRIVATE cxxtrace gtest gtest_main)
add_test(NAME test_cxxtrace COMMAND test_cxxtrace)
