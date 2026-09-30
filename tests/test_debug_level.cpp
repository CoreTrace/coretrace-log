#include "check.hpp"

#include <coretrace/logger.hpp>

int main() {
  using namespace coretrace;

  CHECK(ct_test::set_env("CT_LOG_LEVEL", "debug"));

  set_sink(ct_test::capture_sink);
  enable_logging();

  // Env defaults should allow DEBUG before explicit API override.
  log(Level::Debug, "debug via env\n");

  set_min_level(Level::Info);
  log(Level::Debug, "debug filtered by info\n");
  log(Level::Info, "info still visible\n");

  reset_sink();

  CHECK(ct_test::logged("debug via env"));
  CHECK(!ct_test::logged("debug filtered by info"));
  CHECK(ct_test::logged("info still visible"));
  return ct_test::result();
}
