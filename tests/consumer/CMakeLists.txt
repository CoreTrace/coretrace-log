# Minimal project that consumes coretrace-logger the way users do.
# With -DCORETRACE_LOGGER_SOURCE_DIR=<repo>: through add_subdirectory().
# Otherwise: through find_package() after `cmake --install`.
cmake_minimum_required(VERSION 3.16)
project(coretrace_logger_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CORETRACE_LOGGER_SOURCE_DIR)
  add_subdirectory(${CORETRACE_LOGGER_SOURCE_DIR} coretrace-logger)
  if(CORETRACE_LOGGER_BUILD_TESTS)
    message(FATAL_ERROR "coretrace-logger builds its tests as a subproject")
  endif()
else()
  find_package(coretrace-logger CONFIG REQUIRED)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE coretrace::logger)

enable_testing()
add_test(NAME consumer COMMAND consumer)
set_tests_properties(consumer PROPERTIES
  PASS_REGULAR_EXPRESSION "\\[INFO\\] consumer ok")
