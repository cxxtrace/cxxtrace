cmake_minimum_required(VERSION 3.10)

set(
  CXXTRACE_COMPILE_ERROR_TEST_PATH
  "${CMAKE_CURRENT_LIST_DIR}/../test/compile_error_test.py"
)
function (CXXTRACE_ADD_COMPILE_ERROR_TEST NAME)
  if (NOT CXXTRACE_FILECHECK)
    message(
      AUTHOR_WARNING
      "CXXTRACE_FILECHECK must be set before calling CXXTRACE_ADD_COMPILE_ERROR_TEST"
    )
    return ()
  endif ()

  cmake_parse_arguments("" "" "" TARGET_LINK_LIBRARIES ${ARGN})
  list(LENGTH _UNPARSED_ARGUMENTS _UNPARSED_ARGUMENTS_LENGTH)

  if (${_UNPARSED_ARGUMENTS_LENGTH} GREATER 1)
    message(
      FATAL_ERROR
      "CXXTRACE_ADD_COMPILE_ERROR_TEST expects exactly one source file, but more than one was specified: ${_UNPARSED_ARGUMENTS}"
    )
    return ()
  endif ()
  if (${_UNPARSED_ARGUMENTS_LENGTH} EQUAL 0)
    message(
      FATAL_ERROR
      "CXXTRACE_ADD_COMPILE_ERROR_TEST expects exactly one source file, but none were specified"
    )
    return ()
  endif ()
  set(SOURCE ${_UNPARSED_ARGUMENTS})

  set(TEST_PROJECT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${NAME}_project")

  set(ARGS)
  foreach (TARGET IN LISTS _TARGET_LINK_LIBRARIES)
    list(
      APPEND
      ARGS
      --target-include-directories
      $<TARGET_PROPERTY:${TARGET},INTERFACE_INCLUDE_DIRECTORIES>
    )
    list(APPEND ARGS --target-link-library $<TARGET_LINKER_FILE:${TARGET}>)
  endforeach ()

  set(ARGS_FILE "${TEST_PROJECT_PATH}/args")
  file(WRITE "${ARGS_FILE}" "")
  file(APPEND "${ARGS_FILE}" "--cmake\n${CMAKE_COMMAND}\n")
  file(APPEND "${ARGS_FILE}" "--filecheck\n${CXXTRACE_FILECHECK}\n")
  file(
    APPEND
    "${ARGS_FILE}"
    "--source-file\n${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}\n"
  )
  set(
    GLOBAL_CMAKE_VARIABLES
    CMAKE_CXX_COMPILER
    CMAKE_CXX_STANDARD
    CMAKE_CXX_STANDARD_REQUIRED
    # TODO(strager): Forward more variables.
  )
  foreach (VAR IN LISTS GLOBAL_CMAKE_VARIABLES)
    file(APPEND "${ARGS_FILE}" "--cmake-variable\n${VAR}=${${VAR}}\n")
  endforeach ()

  add_test(
    NAME "${NAME}"
    COMMAND
    python3
    "${CXXTRACE_COMPILE_ERROR_TEST_PATH}"
    @${ARGS_FILE}
    ${ARGS}
  )
endfunction ()
