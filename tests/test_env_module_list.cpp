#include "check.hpp"

#include <coretrace/logger.hpp>

int main() {
  // Spaces around names are ignored, and empty items are skipped.
  CHECK(ct_test::set_env("CT_DEBUG", " alloc ,,\ttrace\t, "));

  CHECK(coretrace::module_is_enabled("alloc"));
  CHECK(coretrace::module_is_enabled("trace"));
  CHECK(!coretrace::module_is_enabled("other"));
  CHECK(!coretrace::module_is_enabled(" "));
  return ct_test::result();
}
