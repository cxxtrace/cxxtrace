cmake_minimum_required(VERSION 3.10)

add_subdirectory(zlib)
foreach (ZLIB_TARGET zlib zlibstatic)
  target_include_directories(${ZLIB_TARGET} INTERFACE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
  target_compile_options(${ZLIB_TARGET} PRIVATE -Wno-implicit-fallthrough)
endforeach ()
