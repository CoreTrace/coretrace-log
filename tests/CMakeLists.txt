if(UNIX)
  add_test(NAME coretrace_logger.test_build_without_llvm
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_build_without_llvm.sh"
      "${PROJECT_SOURCE_DIR}/scripts/build.sh")
endif()

# One process per environment scenario: ctest sets the variables before the
# logger reads them at startup, as a user would in the shell.
add_executable(coretrace_logger_test_env test_env.cpp)
target_link_libraries(coretrace_logger_test_env PRIVATE coretrace_logger)

function(coretrace_logger_add_env_test scenario environment)
  add_test(NAME coretrace_logger.test_env.${scenario}
    COMMAND coretrace_logger_test_env ${scenario})
  set_tests_properties(coretrace_logger.test_env.${scenario}
    PROPERTIES ENVIRONMENT "${environment}")
endfunction()

coretrace_logger_add_env_test(level_debug "CT_LOG_LEVEL=debug")
coretrace_logger_add_env_test(level_warn "CT_LOG_LEVEL=warn")
coretrace_logger_add_env_test(level_error "CT_LOG_LEVEL=error")
coretrace_logger_add_env_test(level_uppercase "CT_LOG_LEVEL=DEBUG")
coretrace_logger_add_env_test(level_invalid "CT_LOG_LEVEL=verbose")
coretrace_logger_add_env_test(level_api_wins "CT_LOG_LEVEL=debug")
coretrace_logger_add_env_test(getters "CT_LOG_LEVEL=debug;CT_DEBUG=selected")
coretrace_logger_add_env_test(debug_list "CT_DEBUG= alloc ,,\ttrace\t, ")
coretrace_logger_add_env_test(debug_too_long
  "CT_DEBUG=alloc,xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx,trace")
coretrace_logger_add_env_test(debug_api_wins "CT_DEBUG=alloc")

add_executable(coretrace_logger_test_stderr_sink test_stderr_sink.cpp)
target_link_libraries(coretrace_logger_test_stderr_sink PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_stderr_sink COMMAND coretrace_logger_test_stderr_sink)
# PASS_REGULAR_EXPRESSION ignores the exit code, so failed checks fail the test
# through FAIL_REGULAR_EXPRESSION instead.
set_tests_properties(coretrace_logger.test_stderr_sink PROPERTIES
  PASS_REGULAR_EXPRESSION "\\[INFO\\] to stderr[\r\n]+[^\r\n]*\\[INFO\\] back to stderr"
  FAIL_REGULAR_EXPRESSION "captured only;CHECK\\(")

add_executable(coretrace_logger_test_module_filter test_module_filter.cpp)
target_link_libraries(coretrace_logger_test_module_filter PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_module_filter COMMAND coretrace_logger_test_module_filter)

add_executable(coretrace_logger_test_module_limits test_module_limits.cpp)
target_link_libraries(coretrace_logger_test_module_limits PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_module_limits COMMAND coretrace_logger_test_module_limits)

add_executable(coretrace_logger_test_unit test_unit.cpp)
target_link_libraries(coretrace_logger_test_unit PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_unit COMMAND coretrace_logger_test_unit)

if(UNIX)
  # Colors need a terminal on stderr: the test attaches a pseudo-terminal. The
  # color decision is cached per process, so each case runs in its own process.
  add_executable(coretrace_logger_test_colors_tty test_colors_tty.cpp)
  target_link_libraries(coretrace_logger_test_colors_tty PRIVATE coretrace_logger)
  add_test(NAME coretrace_logger.test_colors_tty COMMAND coretrace_logger_test_colors_tty)
  add_test(NAME coretrace_logger.test_colors_tty.no_color
    COMMAND coretrace_logger_test_colors_tty no_color)
  set_tests_properties(coretrace_logger.test_colors_tty.no_color
    PROPERTIES ENVIRONMENT "NO_COLOR=1")
endif()

add_executable(coretrace_logger_test_sink_reentrancy test_sink_reentrancy.cpp)
target_link_libraries(coretrace_logger_test_sink_reentrancy PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_sink_reentrancy COMMAND coretrace_logger_test_sink_reentrancy)
set_tests_properties(coretrace_logger.test_sink_reentrancy PROPERTIES TIMEOUT 5)

add_executable(coretrace_logger_test_sink_switch test_sink_switch.cpp)
target_link_libraries(coretrace_logger_test_sink_switch PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_sink_switch COMMAND coretrace_logger_test_sink_switch)

add_executable(coretrace_logger_test_concurrency_smoke test_concurrency_smoke.cpp)
target_link_libraries(coretrace_logger_test_concurrency_smoke PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_concurrency_smoke COMMAND coretrace_logger_test_concurrency_smoke)
set_tests_properties(coretrace_logger.test_concurrency_smoke PROPERTIES TIMEOUT 20)

add_executable(coretrace_logger_test_concurrent_lines test_concurrent_lines.cpp)
target_link_libraries(coretrace_logger_test_concurrent_lines PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_concurrent_lines COMMAND coretrace_logger_test_concurrent_lines)
set_tests_properties(coretrace_logger.test_concurrent_lines PROPERTIES TIMEOUT 20)

add_executable(coretrace_logger_test_format_error_serialized test_format_error_serialized.cpp)
target_link_libraries(coretrace_logger_test_format_error_serialized PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_format_error_serialized COMMAND coretrace_logger_test_format_error_serialized)
set_tests_properties(coretrace_logger.test_format_error_serialized PROPERTIES TIMEOUT 20)

add_executable(coretrace_logger_test_sink_swap_waits test_sink_swap_waits.cpp)
target_link_libraries(coretrace_logger_test_sink_swap_waits PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_sink_swap_waits COMMAND coretrace_logger_test_sink_swap_waits)
set_tests_properties(coretrace_logger.test_sink_swap_waits PROPERTIES TIMEOUT 20)

if(UNIX)
  add_test(NAME coretrace_logger.test_build_clean_guard
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_build_clean_guard.sh"
      "${PROJECT_SOURCE_DIR}/scripts/build.sh")
endif()

if(TARGET coretrace_logger_example)
  add_test(NAME coretrace_logger.example COMMAND coretrace_logger_example)
  set_tests_properties(coretrace_logger.example PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[WARN\\] This WARN should appear"
    FAIL_REGULAR_EXPRESSION "should NOT appear")
endif()

# Compile-only: the build fails when a README example stops compiling.
add_library(coretrace_logger_readme_snippets OBJECT readme_snippets.cpp)
target_link_libraries(coretrace_logger_readme_snippets PRIVATE coretrace_logger)
