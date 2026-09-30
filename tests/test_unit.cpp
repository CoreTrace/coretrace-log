#include "check.hpp"

#include <coretrace/logger.hpp>

#include <cstddef>
#include <cstdint>
#include <regex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

using namespace coretrace;
using ct_test::captured;
using ct_test::logged;

// Restores the default configuration and empties the capture between cases.
void reset() {
  captured.clear();
  set_sink(ct_test::capture_sink);
  enable_logging();
  set_min_level(Level::Info);
  enable_all_modules();
  set_prefix("==ct==");
  set_timestamps(false);
  set_source_location(false);
}

// "|<pid>| ==ct== [<LEVEL>]", the start of every line with the default prefix.
std::string prefix_for(std::string_view level) {
  return "|" + std::to_string(pid()) + "| ==ct== [" + std::string(level) + "]";
}

int current_process_id() {
#if defined(_WIN32)
  return _getpid();
#else
  return static_cast<int>(getpid());
#endif
}

void disabled_by_default() {
  set_sink(ct_test::capture_sink);
  CHECK(!log_is_enabled());
  log(Level::Error, "before enable\n");
  CHECK(captured.empty());
}

void line_format() {
  reset();
  log(Level::Info, "plain\n");
  CHECK(captured == prefix_for("INFO") + " plain\n");

  reset();
  log(Level::Warn, Module("alloc"), "tagged\n");
  CHECK(captured == prefix_for("WARN") + " (alloc) tagged\n");

  reset();
  set_source_location(true);
  const auto here = std::source_location::current();
  log(LogEntry(Level::Error, here), "located\n");
  CHECK(captured == prefix_for("ERROR") + " test_unit.cpp:" +
                        std::to_string(here.line()) + " located\n");

  reset();
  set_timestamps(true);
  log(Level::Info, "stamped\n");
  // [YYYY-MM-DDThh:mm:ss.mmm] followed by a space: 26 characters.
  const std::string stamp = captured.substr(0, 26);
  const std::string rest = captured.substr(stamp.size());
  CHECK(std::regex_match(
      stamp, std::regex(R"(\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}\] )")));
  CHECK(rest == prefix_for("INFO") + " stamped\n");

  reset();
  set_prefix("==app==");
  log(Level::Info, "custom\n");
  CHECK(captured == "|" + std::to_string(pid()) + "| ==app== [INFO] custom\n");

  reset();
  set_prefix(std::string(100, 'p'));
  log(Level::Info, "cut\n");
  CHECK(captured == "|" + std::to_string(pid()) + "| " + std::string(63, 'p') +
                        " [INFO] cut\n");
}

void level_filtering() {
  const Level levels[] = {Level::Debug, Level::Info, Level::Warn, Level::Error};
  for (Level min : levels) {
    reset();
    set_min_level(min);
    CHECK(min_level() == min);
    for (Level level : levels)
      log(level, "{}\n", level_label(level));
    for (Level level : levels)
      CHECK(logged("] " + std::string(level_label(level)) + "\n") ==
            (level >= min));
  }

  CHECK(level_label(Level::Debug) == "DEBUG");
  CHECK(level_label(Level::Info) == "INFO");
  CHECK(level_label(Level::Warn) == "WARN");
  CHECK(level_label(Level::Error) == "ERROR");
}

void disable_logging_stops_output() {
  reset();
  disable_logging();
  CHECK(!log_is_enabled());
  log(Level::Error, "after disable\n");
  CHECK(captured.empty());
}

void module_table() {
  reset();
  enable_module("a");
  enable_module("a"); // Stored once: one disable_module("a") removes it.
  enable_module("b");
  enable_module("c");

  disable_module("b"); // Middle entry.
  CHECK(module_is_enabled("a"));
  CHECK(!module_is_enabled("b"));
  CHECK(module_is_enabled("c"));

  disable_module("unknown");
  CHECK(module_is_enabled("a"));
  CHECK(module_is_enabled("c"));

  disable_module("a"); // First entry.
  CHECK(!module_is_enabled("a"));
  CHECK(module_is_enabled("c"));

  disable_module("c"); // Last entry.
  CHECK(!module_is_enabled("c"));

  // An empty module name bypasses the filter and prints no tag.
  log(Level::Info, Module(""), "no tag\n");
  CHECK(captured == prefix_for("INFO") + " no tag\n");
}

void error_paths() {
  reset();
  log(Level::Info, "{} {}\n", 1); // Missing argument.
  CHECK(captured == "coretrace: log format error\n");

  reset();
  log(Level::Info, "");
  log(Level::Info, "{}", "");
  write_raw(nullptr, 4);
  write_raw("data", 0);
  write_str("");
  CHECK(captured.empty());
}

void low_level_writers() {
  reset();
  write_dec(0);
  write_str(" ");
  write_dec(42);
  write_str(" ");
  write_dec(SIZE_MAX);
  CHECK(captured == "0 42 " + std::to_string(SIZE_MAX));

  reset();
  write_hex(0);
  write_str(" ");
  write_hex(0xdead);
  write_str(" ");
  write_hex(UINTPTR_MAX);
  CHECK(captured ==
        "0x0 0xdead 0x" + std::string(sizeof(std::uintptr_t) * 2, 'f'));

  reset();
  write_prefix(Level::Warn);
  CHECK(captured == prefix_for("WARN") + " ");
}

void system_info() {
  CHECK(pid() == current_process_id());

  unsigned long long other_thread = 0;
  std::thread worker([&other_thread] { other_thread = thread_id(); });
  worker.join();
  CHECK(thread_id() != 0);
  CHECK(other_thread != thread_id());
}

void colors_disabled() {
  for (int c = static_cast<int>(Color::Reset);
       c <= static_cast<int>(Color::BgBrightWhite); ++c)
    CHECK(color(static_cast<Color>(c)).empty());
  CHECK(level_color(Level::Error).empty());
}

} // namespace

int main() {
  // Keep the output free of color codes, even when stderr is a terminal.
  CHECK(ct_test::set_env("NO_COLOR", "1"));

  disabled_by_default(); // Must run before anything enables logging.
  line_format();
  level_filtering();
  disable_logging_stops_output();
  module_table();
  error_paths();
  low_level_writers();
  system_info();
  colors_disabled();

  reset_sink();
  return ct_test::result();
}
