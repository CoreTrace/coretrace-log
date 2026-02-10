cmake_minimum_required(VERSION 3.16)
project(coretrace-logger VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)

### Library ###

add_library(coretrace_logger STATIC src/logger.cpp)

target_include_directories(coretrace_logger
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(coretrace_logger
  PUBLIC
    Threads::Threads
)

# Alias for use with FetchContent / add_subdirectory.
add_library(coretrace::logger ALIAS coretrace_logger)
set_target_properties(coretrace_logger PROPERTIES EXPORT_NAME logger)

### Example ###

option(CORETRACE_LOGGER_BUILD_EXAMPLES "Build example programs" ON)

if(CORETRACE_LOGGER_BUILD_EXAMPLES)
  add_executable(coretrace_logger_example example/main.cpp)
  target_link_libraries(coretrace_logger_example PRIVATE coretrace_logger)
endif()

### Tests ###

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CORETRACE_LOGGER_IS_TOP_LEVEL ON)
else()
  set(CORETRACE_LOGGER_IS_TOP_LEVEL OFF)
endif()

option(CORETRACE_LOGGER_BUILD_TESTS "Build test programs" ${CORETRACE_LOGGER_IS_TOP_LEVEL})

if(CORETRACE_LOGGER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

### Install ###

set(CORETRACE_LOGGER_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/coretrace-logger")

install(TARGETS coretrace_logger
  EXPORT coretrace_loggerTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT coretrace_loggerTargets
  FILE coretrace-loggerTargets.cmake
  NAMESPACE coretrace::
  DESTINATION ${CORETRACE_LOGGER_CMAKE_DIR}
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/coretrace-loggerConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/coretrace-loggerConfig.cmake"
  INSTALL_DESTINATION ${CORETRACE_LOGGER_CMAKE_DIR}
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/coretrace-loggerConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/coretrace-loggerConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/coretrace-loggerConfigVersion.cmake"
  DESTINATION ${CORETRACE_LOGGER_CMAKE_DIR}
)
