// Compile-only copies of the C++ examples in README.md: the build fails when
// an API change breaks one of them. Keep each function in sync with the README
// section it is named after.
#include <coretrace/logger.hpp>

#include <cstddef>
#include <cstdio>

namespace {

using coretrace::Color;
using coretrace::Level;
using coretrace::Module;

[[maybe_unused]] void quick_start() {
  coretrace::enable_logging();
  coretrace::set_min_level(coretrace::Level::Debug); // optional
  coretrace::log(coretrace::Level::Debug, "boot trace\n");
  coretrace::log(coretrace::Level::Info, "Hello {}\n", "world");
  coretrace::log(coretrace::Level::Warn, "count={}\n", 42);
  coretrace::log(coretrace::Level::Error, "disk full\n");
}

[[maybe_unused]] void core() {
  coretrace::enable_logging();
  coretrace::disable_logging();
  (void)coretrace::log_is_enabled();

  coretrace::set_prefix("==myapp==");
}

[[maybe_unused]] void logging(int value) {
  coretrace::log(Level::Info, "message {}\n", value);
  coretrace::log(Level::Info, Module("alloc"), "malloc size={}\n", 64);
}

[[maybe_unused]] void level_filtering() {
  coretrace::set_min_level(Level::Warn);
  coretrace::set_min_level(Level::Info);
  coretrace::set_min_level(Level::Debug);
}

[[maybe_unused]] void module_filtering() {
  coretrace::enable_module("alloc");
  coretrace::enable_module("trace");

  coretrace::log(Level::Info, Module("alloc"), "tracked\n");
  coretrace::log(Level::Info, Module("network"), "dropped\n");
  coretrace::log(Level::Info, "no module = always printed\n");

  coretrace::enable_all_modules();
}

[[maybe_unused]] void timestamps() { coretrace::set_timestamps(true); }

[[maybe_unused]] void source_location() {
  coretrace::set_source_location(true);
}

[[maybe_unused]] void custom_sink() {
  static FILE *f = fopen("app.log", "w");
  coretrace::set_sink(
      [](const char *data, size_t size) { fwrite(data, 1, size, f); });

  coretrace::reset_sink();
}

[[maybe_unused]] void thread_safety() {
  coretrace::set_thread_safe(true);
  coretrace::set_thread_safe(false);
}

[[maybe_unused]] void colors() {
  coretrace::log(Level::Info, "{}bold{} and {}red{}\n",
                 coretrace::color(Color::Bold), coretrace::color(Color::Reset),
                 coretrace::color(Color::Red), coretrace::color(Color::Reset));
}

[[maybe_unused]] void low_level_api(const char *buf, size_t len) {
  coretrace::write_prefix(Level::Info);
  coretrace::write_str("hello");
  coretrace::write_dec(42);
  coretrace::write_hex(0xDEAD);
  coretrace::write_raw(buf, len);
  (void)coretrace::pid();
  (void)coretrace::thread_id();
}

} // namespace
