cmake_minimum_required(VERSION 3.10)

add_subdirectory(test_ring_queue)

add_executable(
  test_cxxtrace
  exhaustive_rng.cpp
  test_add.cpp
  test_exhaustive_rng.cpp
  test_ring_queue.cpp
  test_span.cpp
  test_thread.cpp
)
target_link_libraries(test_cxxtrace PRIVATE cxxtrace gmock gtest gmock_main)
add_test(NAME test_cxxtrace COMMAND test_cxxtrace)

add_executable(benchmark_cxxtrace benchmark_span.cpp)
target_link_libraries(benchmark_cxxtrace PRIVATE benchmark benchmark_main cxxtrace)
