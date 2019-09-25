cmake_minimum_required(VERSION 3.10)

include(FindPackageHandleStandardArgs)
include(cxxtrace_check_cxx_source_compiles)

function (CXXTRACE_FIND_AND_ADD_LIBRARY)
  if (TARGET CxxtraceStdFilesystem)
    set(ADD_LIBRARY FALSE)
  else ()
    set(ADD_LIBRARY TRUE)
  endif ()

  if (CxxtraceStdFilesystem_FIND_QUIETLY)
    set(QUIET_OPTIONS QUIET)
  else ()
    set(QUIET_OPTIONS)
  endif ()

  if (ADD_LIBRARY)
    add_library(CxxtraceStdFilesystem INTERFACE)
  endif ()

  set(STD_FILESYSTEM_SOURCE "#include <cstdio>
#include <filesystem>
#include <system_error>

int main() {
  try {
    auto exists = std::filesystem::exists(std::filesystem::path{});
    if (!exists) {
      throw std::filesystem::filesystem_error{
        \"file does not exist\", std::error_code{}};
    }
    return 0;
  } catch (std::filesystem::filesystem_error& e) {
    std::fputs(e.what(), stderr);
    return 2;
  }
}
")

  cxxtrace_check_cxx_source_compiles(
    "${STD_FILESYSTEM_SOURCE}"
    CXXTRACE_HAS_STD_FILESYSTEM
    ${QUIET_OPTIONS}
  )
  if (CXXTRACE_HAS_STD_FILESYSTEM)
    set(CxxtraceStdFilesystem_FOUND TRUE PARENT_SCOPE)
    return ()
  endif ()

  cxxtrace_check_cxx_source_compiles(
    "${STD_FILESYSTEM_SOURCE}"
    CXXTRACE_HAS_STD_FILESYSTEM_WITH_LIBSTDCXXFS
    REQUIRED_LIBRARIES stdc++fs
    ${QUIET_OPTIONS}
  )
  if (CXXTRACE_HAS_STD_FILESYSTEM_WITH_LIBSTDCXXFS)
    set(CxxtraceStdFilesystem_FOUND TRUE PARENT_SCOPE)
    if (ADD_LIBRARY)
      target_link_libraries(CxxtraceStdFilesystem INTERFACE stdc++fs)
    endif ()
    return ()
  endif ()

  set(CxxtraceStdFilesystem_FOUND FALSE PARENT_SCOPE)
endfunction ()
cxxtrace_find_and_add_library()

find_package_handle_standard_args(
  CxxtraceStdFilesystem
  REQUIRED_VARS CxxtraceStdFilesystem_FOUND
)
