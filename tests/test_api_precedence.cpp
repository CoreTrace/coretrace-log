#include "check.hpp"

#include <coretrace/logger.hpp>

int main() {
  CHECK(ct_test::set_env("CT_LOG_LEVEL", "info"));

  coretrace::set_sink(ct_test::capture_sink);
  coretrace::enable_logging();

  // Explicit API config must win over env defaults.
  coretrace::set_min_level(coretrace::Level::Error);

  coretrace::log(coretrace::Level::Warn, "warn should be filtered\n");
  coretrace::log(coretrace::Level::Error, "error should pass\n");

  coretrace::reset_sink();

  CHECK(!ct_test::logged("[WARN]"));
  CHECK(ct_test::logged("[ERROR]"));
  return ct_test::result();
}
