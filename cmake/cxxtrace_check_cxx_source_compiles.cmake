include(CheckCXXSourceCompiles)

function (CXXTRACE_CHECK_CXX_SOURCE_COMPILES SOURCE OUT_VAR)
  cmake_parse_arguments("" QUIET "" REQUIRED_LIBRARIES ${ARGN})

  list(LENGTH _UNPARSED_ARGUMENTS _UNPARSED_ARGUMENTS_LENGTH)
  if (${_UNPARSED_ARGUMENTS_LENGTH} GREATER 0)
    message(
      FATAL_ERROR
      "CXXTRACE_CHECK_CXX_SOURCE_COMPILES got unexpected arguments: ${_UNPARSED_ARGUMENTS}"
    )
    return ()
  endif ()

  list(APPEND CMAKE_REQUIRED_LIBRARIES ${_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_QUIET ${_QUIET})

  # HACK(strager): Propagate CMAKE_CXX_STANDARD and CMAKE_CXX_STANDARD_REQUIRED.
  # for some reason, check_cxx_source_compiles doesn't do this automatically.
  list(
    APPEND
    CMAKE_REQUIRED_LIBRARIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ${CMAKE_CXX_STANDARD_REQUIRED}
  )

  check_cxx_source_compiles("${SOURCE}" "${OUT_VAR}")
  set("${OUT_VAR}" "${${OUT_VAR}}" PARENT_SCOPE)
endfunction ()
