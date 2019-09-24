cmake_minimum_required(VERSION 3.10)

if (CXXTRACE_CDSCHECKER)
  # See CDSChecker/test/Makefile and CDSChecker/common.mk.
  add_library(CDSChecker_common INTERFACE)
  target_include_directories(
    CDSChecker_common
    INTERFACE
    CDSChecker
    CDSChecker/include
  )
  if (APPLE)
    # See CDSChecker/test/Makefile and CDSChecker/common.mk.
    target_compile_definitions(CDSChecker_common INTERFACE MAC=1 _XOPEN_SOURCE=1)
  endif ()
  add_library(
    CDSChecker_model
    SHARED
    # See CDSChecker/Makefile and CDSChecker/sfence/Makefile.
    CDSChecker/action.cc
    CDSChecker/clockvector.cc
    CDSChecker/cmodelint.cc
    CDSChecker/common.cc
    CDSChecker/conditionvariable.cc
    CDSChecker/context.cc
    CDSChecker/cyclegraph.cc
    CDSChecker/datarace.cc
    CDSChecker/execution.cc
    CDSChecker/impatomic.cc
    CDSChecker/libannotate.cc
    CDSChecker/librace.cc
    CDSChecker/libthreads.cc
    CDSChecker/main.cc
    CDSChecker/malloc.c
    CDSChecker/model.cc
    CDSChecker/mutex.cc
    CDSChecker/mymemory.cc
    CDSChecker/nodestack.cc
    CDSChecker/plugins.cc
    CDSChecker/promise.cc
    CDSChecker/scanalysis.cc
    CDSChecker/scfence/inference.cc
    CDSChecker/scfence/inferlist.cc
    CDSChecker/scfence/inferset.cc
    CDSChecker/scfence/patch.cc
    CDSChecker/scfence/scfence.cc
    CDSChecker/scfence/scgen.cc
    CDSChecker/schedule.cc
    CDSChecker/snapshot-interface.cc
    CDSChecker/snapshot.cc
    CDSChecker/threads.cc
  )
  # See CDSChecker/Makefile.
  set_property(
    SOURCE CDSChecker/malloc.c
    APPEND PROPERTY
    COMPILE_DEFINITIONS
    HAVE_MMAP=0
    MSPACES=1
    ONLY_MSPACES=1
  )
  # See CDSChecker/Makefile.
  target_include_directories(
    CDSChecker_model
    PRIVATE
    CDSChecker/scfence
  )
  # See CDSChecker/Makefile.
  target_link_libraries(
    CDSChecker_model
    PUBLIC -rdynamic
    PRIVATE CDSChecker_common dl
  )
  if (APPLE)
    # See CDSChecker/Makefile.
    set_property(
      TARGET CDSChecker_model
      APPEND_STRING PROPERTY
      LINK_FLAGS
      " -Wl,-undefined,dynamic_lookup"
    )
  else ()
    # See CDSChecker/Makefile.
    target_link_libraries(CDSChecker_model PRIVATE rt)
  endif ()
  target_compile_options(
    CDSChecker_model
    PRIVATE
    -Wno-deprecated-declarations
    -Wno-dynamic-exception-spec
    -Wno-gnu-statement-expression
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-ignored-qualifiers
    -Wno-implicit-exception-spec-mismatch
    -Wno-implicit-fallthrough
    -Wno-unknown-warning-option
    -Wno-unused-but-set-variable
    -Wno-unused-parameter
    -Wno-unused-private-field
    -Wno-vla-extension
  )
  set_property(
    SOURCE CDSChecker/mymemory.cc
    APPEND_STRING PROPERTY
    COMPILE_FLAGS
    " -Wno-sized-deallocation"
  )
  set_property(
    SOURCE CDSChecker/malloc.c
    APPEND_STRING PROPERTY
    COMPILE_FLAGS
    " -Wno-empty-body -Wno-expansion-to-defined -Wno-null-pointer-arithmetic -Wno-sign-compare -Wno-unknown-warning-option -Wno-unused-variable"
  )
  # Allow dynamic exception specifications (deprecated in C++17).
  set_property(
    SOURCE CDSChecker/mymemory.cc
    APPEND_STRING PROPERTY
    COMPILE_FLAGS
    " -Wno-deprecated"
  )
  set_property(TARGET CDSChecker_model PROPERTY CXX_STANDARD 14)
endif ()

set(BENCHMARK_DOWNLOAD_DEPENDENCIES FALSE CACHE INTERNAL "")
set(BENCHMARK_ENABLE_ASSEMBLY_TESTS FALSE CACHE INTERNAL "")
set(BENCHMARK_ENABLE_EXCEPTIONS TRUE CACHE INTERNAL "")
set(BENCHMARK_ENABLE_GTEST_TESTS FALSE CACHE INTERNAL "")
set(BENCHMARK_ENABLE_INSTALL FALSE CACHE INTERNAL "")
set(BENCHMARK_ENABLE_LTO "${CMAKE_INTERPROCEDURAL_OPTIMIZATION}" CACHE INTERNAL "")
set(BENCHMARK_ENABLE_TESTING FALSE CACHE INTERNAL "")
add_subdirectory(benchmark)

set(BUILD_GMOCK TRUE CACHE INTERNAL "")
set(INSTALL_GTEST FALSE)
set(gmock_build_tests FALSE CACHE INTERNAL "")
add_subdirectory(googletest)

set(JSON_BuildTests FALSE CACHE INTERNAL "")
set(JSON_Install FALSE CACHE INTERNAL "")
set(JSON_MultipleHeaders TRUE CACHE INTERNAL "")
add_subdirectory(nlohmann-json)

add_library(relacy INTERFACE)
target_include_directories(relacy INTERFACE relacy)
if (APPLE)
  # Allow including <ucontext.h>, and fix the definition of ucontext_t.
  target_compile_definitions(relacy INTERFACE _XOPEN_SOURCE=700)
endif ()
