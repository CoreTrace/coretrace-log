#include "check.hpp"

#include <coretrace/logger.hpp>

#include <string>

int main() {
  using coretrace::enable_module;

  // Names of 32 characters or more do not fit the table.
  CHECK(enable_module(std::string(31, 'x')));
  CHECK(!enable_module(std::string(32, 'y')));
  CHECK(!enable_module(""));

  // The table holds 32 names. Enabling a stored name again still succeeds.
  for (int i = 1; i < 32; ++i)
    CHECK(enable_module("m" + std::to_string(i)));
  CHECK(!enable_module("one-too-many"));
  CHECK(enable_module("m1"));
  CHECK(!coretrace::module_is_enabled("one-too-many"));

  return ct_test::result();
}
