#include "check.hpp"

#include <coretrace/logger.hpp>

#include <cstddef>

int main() {
  using namespace coretrace;
  using ct_test::logged;

  set_sink(ct_test::capture_sink);
  enable_logging();
  set_min_level(Level::Info);

  enable_all_modules();
  enable_module("alloc");
  enable_module("trace");

  log(Level::Info, Module("alloc"), "alloc accepted\n");
  log(Level::Info, Module("network"), "network filtered\n");
  log(Level::Info, "untagged accepted\n");

  CHECK(logged("alloc accepted"));
  CHECK(!logged("network filtered"));
  CHECK(logged("untagged accepted"));

  disable_module("alloc");

  const size_t before = ct_test::captured.size();
  log(Level::Info, Module("alloc"), "alloc filtered\n");
  log(Level::Info, Module("trace"), "trace accepted\n");

  CHECK(!logged("alloc filtered", before));
  CHECK(logged("trace accepted", before));

  reset_sink();
  return ct_test::result();
}
