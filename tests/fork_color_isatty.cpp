// isatty() for test_fork_color.cpp, linked into the test program ahead of the
// C library. The logger calls it to decide whether stderr takes colors: each
// call lasts long enough for the test to fork during it.

#include <atomic>
#include <time.h>
#include <unistd.h>

std::atomic<bool> g_deciding_colors{false};

// Same exception specification as the C library's declaration, which glibc
// marks noexcept.
extern "C" int isatty(int) noexcept(noexcept(::isatty(0))) {
  g_deciding_colors.store(true);
  const timespec delay{0, 300 * 1000 * 1000};
  nanosleep(&delay, nullptr);
  return 0;
}
