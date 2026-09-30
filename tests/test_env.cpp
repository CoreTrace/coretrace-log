#include "check.hpp"

#include <coretrace/logger.hpp>

#include <cstdio>
#include <string_view>

// Runs one startup scenario per process. ctest sets the environment variables
// of each scenario (see tests/CMakeLists.txt), as a user would in the shell.
int main(int argc, char **argv) {
  using namespace coretrace;
  using ct_test::logged;

  const std::string_view scenario = argc > 1 ? argv[1] : "";
  set_sink(ct_test::capture_sink);
  enable_logging();

  if (scenario == "level_debug") { // CT_LOG_LEVEL=debug
    CHECK(min_level() == Level::Debug);
    log(Level::Debug, "debug via env\n");
    set_min_level(Level::Info);
    log(Level::Debug, "debug filtered by info\n");
    CHECK(logged("debug via env"));
    CHECK(!logged("debug filtered by info"));
  } else if (scenario == "level_warn") { // CT_LOG_LEVEL=warn
    CHECK(min_level() == Level::Warn);
    log(Level::Info, "info filtered\n");
    log(Level::Warn, "warn passes\n");
    CHECK(!logged("info filtered"));
    CHECK(logged("warn passes"));
  } else if (scenario == "level_error") { // CT_LOG_LEVEL=error
    CHECK(min_level() == Level::Error);
  } else if (scenario == "level_uppercase") { // CT_LOG_LEVEL=DEBUG
    CHECK(min_level() == Level::Debug);
  } else if (scenario == "level_invalid") { // CT_LOG_LEVEL=verbose
    CHECK(min_level() == Level::Info);
  } else if (scenario == "level_api_wins") { // CT_LOG_LEVEL=debug
    set_min_level(Level::Error);
    log(Level::Warn, "warn filtered\n");
    log(Level::Error, "error passes\n");
    CHECK(min_level() == Level::Error);
    CHECK(!logged("warn filtered"));
    CHECK(logged("error passes"));
  } else if (scenario == "getters") { // CT_LOG_LEVEL=debug CT_DEBUG=selected
    // Getters must load the environment before any log call or setter.
    CHECK(min_level() == Level::Debug);
    CHECK(module_is_enabled("selected"));
    CHECK(!module_is_enabled("other"));
  } else if (scenario == "debug_list") {
    // CT_DEBUG=" alloc ,,<tab>trace<tab>, "
    CHECK(module_is_enabled("alloc"));
    CHECK(module_is_enabled("trace"));
    CHECK(!module_is_enabled("other"));
    CHECK(!module_is_enabled(" "));
  } else if (scenario == "debug_too_long") { // CT_DEBUG=alloc,<32 x>,trace
    CHECK(module_is_enabled("alloc"));
    CHECK(module_is_enabled("trace"));
    CHECK(!module_is_enabled("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"));
  } else if (scenario == "debug_api_wins") { // CT_DEBUG=alloc
    enable_module("trace");
    CHECK(module_is_enabled("trace"));
    CHECK(!module_is_enabled("alloc"));
  } else {
    std::fprintf(stderr, "unknown scenario: '%.*s'\n",
                 static_cast<int>(scenario.size()), scenario.data());
    return 1;
  }

  reset_sink();
  return ct_test::result();
}
