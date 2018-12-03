cmake_minimum_required(VERSION 3.10)
enable_language(ASM)

add_subdirectory(test_ring_queue)

add_executable(
  test_cxxtrace
  exhaustive_rng.cpp
  test_add.cpp
  test_exhaustive_rng.cpp
  test_molecular.cpp
  test_ring_queue.cpp
  test_span.cpp
  test_thread.cpp
)
target_link_libraries(test_cxxtrace PRIVATE cxxtrace gmock gtest gmock_main)
add_test(NAME test_cxxtrace COMMAND test_cxxtrace)

add_library(cxxtrace_concurrency_test cxxtrace_concurrency_test.cpp)

if (CXXTRACE_CDSCHECKER)
  add_library(
    cxxtrace_test_concurrency_cdschecker_main
    OBJECT
    cxxtrace_concurrency_test_main_cdschecker.cpp
  )
  target_include_directories(
    cxxtrace_test_concurrency_cdschecker_main
    PRIVATE
    $<TARGET_PROPERTY:cxxtrace,INTERFACE_INCLUDE_DIRECTORIES>
  )

  add_executable(
    cxxtrace_test_ring_queue_concurrency_cdschecker
    cxxtrace_concurrency_test_cdschecker.cpp
    test_ring_queue_concurrency.cpp
    $<TARGET_OBJECTS:cxxtrace_test_concurrency_cdschecker_main>
  )
  target_compile_definitions(
    cxxtrace_test_ring_queue_concurrency_cdschecker
    PRIVATE
    CXXTRACE_ENABLE_CDSCHECKER=1
  )
  target_link_libraries(
    cxxtrace_test_ring_queue_concurrency_cdschecker
    PRIVATE
    CDSChecker_model
    cxxtrace
    cxxtrace_concurrency_test
  )
  add_test(
    NAME cxxtrace_test_ring_queue_concurrency_cdschecker
    COMMAND cxxtrace_test_ring_queue_concurrency_cdschecker
  )
endif ()

add_executable(
  cxxtrace_test_ring_queue_concurrency_relacy
  cxxtrace_concurrency_test_main_relacy.cpp
  cxxtrace_concurrency_test_relacy.cpp
  test_ring_queue_concurrency.cpp
)
target_compile_definitions(
  cxxtrace_test_ring_queue_concurrency_relacy
  PRIVATE
  CXXTRACE_ENABLE_RELACY=1
)
target_link_libraries(
  cxxtrace_test_ring_queue_concurrency_relacy
  PRIVATE
  cxxtrace
  cxxtrace_concurrency_test
  relacy
)
add_test(
  NAME cxxtrace_test_ring_queue_concurrency_relacy
  COMMAND cxxtrace_test_ring_queue_concurrency_relacy
)
set_tests_properties(
  cxxtrace_test_ring_queue_concurrency_relacy
  PROPERTIES
  # HACK(strager): Work around ASAN internal crashes.
  ENVIRONMENT ASAN_OPTIONS=detect_container_overflow=0
)

add_executable(
  benchmark_cxxtrace
  benchmark_span.cpp
  benchmark_thread.cpp
  cxxtrace_benchmark.cpp
)
target_link_libraries(benchmark_cxxtrace PRIVATE benchmark benchmark_main cxxtrace)
