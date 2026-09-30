#include "check.hpp"

#include <coretrace/logger.hpp>

// ctest checks what this program writes to stderr (see tests/CMakeLists.txt).
int main() {
  using namespace coretrace;

  enable_logging();
  log(Level::Info, "to stderr\n");

  set_sink(ct_test::capture_sink);
  log(Level::Info, "captured only\n");
  CHECK(ct_test::logged("captured only"));

  set_sink(nullptr); // Back to stderr, like reset_sink().
  log(Level::Info, "back to stderr\n");
  return ct_test::result();
}
