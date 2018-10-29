cmake_minimum_required(VERSION 3.10)

include(cxxtrace_add_compile_error_test)

if (CXXTRACE_FILECHECK)
  set(
    SOURCES
    index_type_must_be_large_enough.cpp
    value_must_have_trivial_type.cpp
  )
  foreach (SOURCE IN LISTS SOURCES)
    cxxtrace_add_compile_error_test(
      "test_ring_queue_compile_error_${SOURCE}"
      "${SOURCE}"
      TARGET_LINK_LIBRARIES cxxtrace
    )
  endforeach ()
endif ()
