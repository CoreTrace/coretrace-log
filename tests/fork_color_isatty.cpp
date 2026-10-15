// isatty() for test_fork_color.cpp, linked into the test program ahead of the
// C library. The logger calls it to decide whether stderr takes colors. The
// first call says stderr is a terminal, after long enough for the test to fork
// and to decide in another thread during it; later calls say it is not, at
// once.

#include <atomic>
#include <time.h>
#include <unistd.h>

std::atomic<bool> g_deciding_colors{false};
std::atomic<int> g_isatty_calls{0};

// Same exception specification as the C library's declaration, which glibc
// marks noexcept.
extern "C" int isatty(int) noexcept(noexcept(::isatty(0))) {
  if (g_isatty_calls.fetch_add(1) != 0)
    return 0;
  g_deciding_colors.store(true);
  const timespec delay{0, 300 * 1000 * 1000};
  nanosleep(&delay, nullptr);
  return 1;
}
