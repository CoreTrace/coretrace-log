// A child of fork() can use the colors although another thread of the parent
// was deciding them at the time of the fork: the child decides them again,
// instead of waiting for a decision that no thread of the child will ever
// finish. In the parent, the first decision stored holds for every thread, and
// later calls do not decide again.

#include "check.hpp"
#include "fork_helpers.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <unistd.h>

extern std::atomic<bool> g_deciding_colors;
extern std::atomic<int> g_isatty_calls;

int main() {
  // The logger decides without isatty() under NO_COLOR, which may come from
  // the environment.
  unsetenv("NO_COLOR");
  std::signal(SIGCHLD, SIG_DFL);

  using coretrace::Color;

  // The decider is told stderr is a terminal, after the main thread is told it
  // is not: the main thread's decision is stored first.
  std::string_view decider_red;
  std::thread decider([&] { decider_red = coretrace::color(Color::Red); });
  const bool deciding = ct_test::wait_for(g_deciding_colors);
  CHECK(deciding);
  if (!deciding) {
    decider.join();
    return ct_test::result();
  }

  const pid_t child = fork();
  if (child == 0) {
    (void)coretrace::color(Color::Red);
    _exit(0);
  }
  const std::string_view main_red = coretrace::color(Color::Red);
  decider.join();
  CHECK(decider_red == main_red);

  const int calls = g_isatty_calls.load();
  CHECK(coretrace::color(Color::Red) == main_red);
  CHECK(g_isatty_calls.load() == calls);

  CHECK(ct_test::child_succeeded(child));
  return ct_test::result();
}
