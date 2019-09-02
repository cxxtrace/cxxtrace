cmake_minimum_required(VERSION 3.10)
enable_language(ASM)

include(cxxtrace_cotire)

add_subdirectory(test_processor_id_manual)
add_subdirectory(test_ring_queue)

add_executable(
  test_cxxtrace
  $<TARGET_OBJECTS:test_cxxtrace_nlohmann_json>
  event.cpp
  exhaustive_rng.cpp
  test_add.cpp
  test_clock.cpp
  test_exhaustive_rng.cpp
  test_molecular.cpp
  test_processor_id.cpp
  test_ring_queue.cpp
  test_snapshot.cpp
  test_span.cpp
  test_span_thread.cpp
  test_thread.cpp
  thread.cpp
)
target_link_libraries(test_cxxtrace PRIVATE cxxtrace gmock gtest gmock_main)
set_target_properties(test_cxxtrace PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(test_cxxtrace PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT pch.h)
cxxtrace_cotire(test_cxxtrace)
add_test(NAME test_cxxtrace COMMAND test_cxxtrace)

add_library(
  test_cxxtrace_nlohmann_json
  OBJECT
  nlohmann_json.cpp
  test_chrome_trace_event_format.cpp
)
target_link_libraries(test_cxxtrace_nlohmann_json PRIVATE cxxtrace gmock gtest gmock_main nlohmann_json)
set_target_properties(test_cxxtrace_nlohmann_json PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
set_target_properties(test_cxxtrace_nlohmann_json PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT pch_nlohmann_json.h)
cxxtrace_cotire(test_cxxtrace_nlohmann_json)

add_library(cxxtrace_concurrency_test cxxtrace_concurrency_test.cpp)
target_link_libraries(cxxtrace_concurrency_test PRIVATE cxxtrace_memory_resource)

if (CXXTRACE_CDSCHECKER)
  add_library(cxxtrace_cdschecker_mutex STATIC cdschecker_mutex.cpp)
  target_compile_definitions(
    cxxtrace_cdschecker_mutex
    PRIVATE
    CXXTRACE_ENABLE_CDSCHECKER=1
  )
  target_link_libraries(
    cxxtrace_cdschecker_mutex
    PRIVATE
    CDSChecker_common
    CDSChecker_model
    cxxtrace
  )
endif ()

function (CXXTRACE_ADD_CONCURRENCY_TESTS BASE_NAME)
  cmake_parse_arguments("" "" "" "" ${ARGN})
  set(SOURCES ${_UNPARSED_ARGUMENTS})

  add_library(${BASE_NAME} INTERFACE)

  if (CXXTRACE_CDSCHECKER)
    add_executable(
      ${BASE_NAME}_cdschecker
      # TODO(strager): Put cxxtrace_concurrency_test_cdschecker.cpp into its own
      # library.
      cxxtrace_concurrency_test_cdschecker.cpp
      ${SOURCES}
      $<TARGET_OBJECTS:cxxtrace_test_concurrency_cdschecker_main>
    )
    target_compile_definitions(
      ${BASE_NAME}_cdschecker
      PRIVATE
      CXXTRACE_ENABLE_CDSCHECKER=1
    )
    target_link_libraries(
      ${BASE_NAME}_cdschecker
      PRIVATE
      ${BASE_NAME}
      CDSChecker_model
      cxxtrace_cdschecker_mutex
      cxxtrace_concurrency_test
    )
    add_test(NAME ${BASE_NAME}_cdschecker COMMAND ${BASE_NAME}_cdschecker)
  endif ()

  add_executable(
    ${BASE_NAME}_relacy
    # TODO(strager): Put cxxtrace_concurrency_test_relacy.cpp and
    # cxxtrace_concurrency_test_main_relacy.cpp into their own library.
    cxxtrace_concurrency_test_main_relacy.cpp
    cxxtrace_concurrency_test_relacy.cpp
    ${SOURCES}
  )
  target_compile_definitions(
    ${BASE_NAME}_relacy
    PRIVATE
    CXXTRACE_ENABLE_RELACY=1
  )
  target_link_libraries(
    ${BASE_NAME}_relacy
    PRIVATE
    ${BASE_NAME}
    cxxtrace_concurrency_test
    relacy
  )
  add_test(NAME ${BASE_NAME}_relacy COMMAND ${BASE_NAME}_relacy)
  set_tests_properties(
    ${BASE_NAME}_relacy
    PROPERTIES
    # HACK(strager): Work around ASAN internal crashes.
    ENVIRONMENT ASAN_OPTIONS=detect_container_overflow=0
  )
endfunction ()

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
endif ()

cxxtrace_add_concurrency_tests(
  cxxtrace_test_ring_queue_concurrency
  test_ring_queue_concurrency.cpp
)
target_link_libraries(cxxtrace_test_ring_queue_concurrency INTERFACE cxxtrace)

add_executable(
  benchmark_cxxtrace
  benchmark_black_hole.cpp
  benchmark_clock.cpp
  benchmark_processor.cpp
  benchmark_ring_queue.cpp
  benchmark_span.cpp
  benchmark_thread.cpp
  cxxtrace_benchmark.cpp
  thread.cpp
)
target_link_libraries(
  benchmark_cxxtrace
  PRIVATE
  benchmark
  benchmark_main
  cxxtrace
  cxxtrace_memory_resource
)

add_library(cxxtrace_memory_resource memory_resource.cpp)
