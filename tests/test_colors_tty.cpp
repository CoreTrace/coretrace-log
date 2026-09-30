#include "check.hpp"

#include <coretrace/logger.hpp>

#include <cstdlib>
#include <fcntl.h>
#include <iterator>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using coretrace::Color;

// SGR code of each Color, in declaration order from Color::Reset.
constexpr int kSgrCodes[] = {
    0,   2,   1,   4,   3,   5,   7,   8,   9, // Reset, Dim ... Strike
    30,  31,  32,  33,  34,  35,  36,  37,     // Black ... White
    90,  91,  92,  93,  94,  95,  96,  97,     // Gray ... BrightWhite
    40,  41,  42,  43,  44,  45,  46,  47,     // BgBlack ... BgWhite
    100, 101, 102, 103, 104, 105, 106, 107,    // BgGray ... BgBrightWhite
};
static_assert(std::size(kSgrCodes) ==
              static_cast<size_t>(Color::BgBrightWhite) + 1);

std::string sgr(int code) { return "\x1b[" + std::to_string(code) + "m"; }

// Makes file descriptor 2 a pseudo-terminal. Returns the previous stderr.
int attach_pty_to_stderr() {
  const int saved_stderr = dup(2);
  const int master = posix_openpt(O_RDWR | O_NOCTTY);
  CHECK(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
  const char *slave_name = master >= 0 ? ptsname(master) : nullptr;
  const int slave = slave_name ? open(slave_name, O_RDWR | O_NOCTTY) : -1;
  CHECK(slave >= 0 && dup2(slave, 2) == 2);
  return saved_stderr;
}

} // namespace

// The logger decides once per process whether stderr gets colors, so ctest
// runs one process with NO_COLOR=1 ("no_color") and one without it.
int main(int argc, char **argv) {
  using namespace coretrace;

  const bool no_color = argc > 1 && std::string_view(argv[1]) == "no_color";
  if (!no_color)
    unsetenv("NO_COLOR");

  set_sink(ct_test::capture_sink);
  enable_logging();

  const int saved_stderr = attach_pty_to_stderr();
  log(Level::Info, "colored\n"); // The color decision happens here.
  dup2(saved_stderr, 2);         // Report failures on the real stderr.

  if (no_color) {
    CHECK(ct_test::captured.find('\x1b') == std::string::npos);
    CHECK(color(Color::Red).empty());
  } else {
    CHECK(ct_test::logged(sgr(32) + "[INFO]" + sgr(0)));
    for (int c = 0; c <= static_cast<int>(Color::BgBrightWhite); ++c)
      CHECK(color(static_cast<Color>(c)) == sgr(kSgrCodes[c]));
    CHECK(level_color(Level::Debug) == sgr(36));
    CHECK(level_color(Level::Info) == sgr(32));
    CHECK(level_color(Level::Warn) == sgr(33));
    CHECK(level_color(Level::Error) == sgr(31));
  }

  reset_sink();
  return ct_test::result();
}
