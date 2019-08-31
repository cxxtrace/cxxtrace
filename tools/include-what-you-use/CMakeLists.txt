cmake_minimum_required(VERSION 3.10)

set(
  CXXTRACE_APPLY_IWYU_PATH
  "${CMAKE_CURRENT_LIST_DIR}/apply_iwyu.py"
)
if (CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(
    include-what-you-use
    COMMAND
    python3
    "${CXXTRACE_APPLY_IWYU_PATH}"
    --cmake-build-directory
    "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    VERBATIM
  )
else ()
  add_custom_target(
    include-what-you-use
    COMMAND
    "${CMAKE}" -E echo "fatal: CMAKE_EXPORT_COMPILE_COMMANDS is unset"
    && "${CMAKE}" -E echo "note: include-what-you-use requires CMAKE_EXPORT_COMPILE_COMMANDS"
    && exit 1
    VERBATIM
  )
endif ()
