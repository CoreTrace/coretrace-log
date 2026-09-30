#include "check.hpp"

#include <coretrace/logger.hpp>

int main() {
  CHECK(ct_test::set_env("CT_LOG_LEVEL", "debug"));
  CHECK(ct_test::set_env("CT_DEBUG", "selected"));

  // Getters must load the environment defaults before any log call or setter.
  CHECK(coretrace::min_level() == coretrace::Level::Debug);
  CHECK(coretrace::module_is_enabled("selected"));
  CHECK(!coretrace::module_is_enabled("other"));
  return ct_test::result();
}
