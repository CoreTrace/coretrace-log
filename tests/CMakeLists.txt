    add_executable(coretrace_logger_test_api_precedence test_api_precedence.cpp)
target_link_libraries(coretrace_logger_test_api_precedence PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_api_precedence COMMAND coretrace_logger_test_api_precedence)

if(UNIX)
  add_test(NAME coretrace_logger.test_build_without_llvm
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/test_build_without_llvm.sh"
      "${PROJECT_SOURCE_DIR}/scripts/build.sh")
endif()

add_executable(coretrace_logger_test_debug_level test_debug_level.cpp)
target_link_libraries(coretrace_logger_test_debug_level PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_debug_level COMMAND coretrace_logger_test_debug_level)

add_executable(coretrace_logger_test_env_getters test_env_getters.cpp)
target_link_libraries(coretrace_logger_test_env_getters PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_env_getters COMMAND coretrace_logger_test_env_getters)

add_executable(coretrace_logger_test_module_filter test_module_filter.cpp)
target_link_libraries(coretrace_logger_test_module_filter PRIVATE coretrace_logger)
add_test(NAME coretrace_logger.test_module_filter COMMAND coretrace_logger_test_module_filter)

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
