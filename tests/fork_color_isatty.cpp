// isatty() for test_fork_color.cpp. The logger calls it once, to decide whether
// stderr takes colors: this definition, linked into the test program ahead of
// the C library, makes that decision last long enough for the test to fork
// during it. Kept apart from <unistd.h>, whose declaration of isatty() may
// carry an exception specification.

#include <atomic>
#include <time.h>

std::atomic<bool> g_deciding_colors{false};

extern "C" int isatty(int) {
  g_deciding_colors.store(true);
  const timespec delay{0, 300 * 1000 * 1000};
  nanosleep(&delay, nullptr);
  return 0;
}
