cmake_minimum_required(VERSION 3.10)

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

# TODO(strager): Do we need to check if pkg-config exists before calling
# pkg_check_modules?
pkg_check_modules(
  Librseq_PKG_CONFIG
  QUIET
  librseq
  IMPORTED_TARGET GLOBAL
)
if (TARGET PkgConfig::Librseq_PKG_CONFIG)
  add_library(Librseq::librseq ALIAS PkgConfig::Librseq_PKG_CONFIG)
  set(Librseq_VERSION ${Librseq_PKG_CONFIG_VERSION})
  find_package_handle_standard_args(
    Librseq
    REQUIRED_VARS Librseq_PKG_CONFIG_FOUND
    VERSION_VAR Librseq_VERSION
  )
else ()
  # TODO(strager): Implement a fallback if pkg-config is unavailable.
  find_package_handle_standard_args(
    Librseq
    REQUIRED_VARS Librseq_PKG_CONFIG_FOUND
  )
endif ()
