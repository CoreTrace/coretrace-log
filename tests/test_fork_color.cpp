// A child of fork() can use the colors although another thread of the parent
// was deciding them at the time of the fork: the child decides them again,
// instead of waiting for a decision that no thread of the child will ever
// finish.

#include "check.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern std::atomic<bool> g_deciding_colors;

namespace {

constexpr std::chrono::seconds kTimeout{5};

// True when `child` exited, with its status in `status`; a child still running
// after kTimeout is killed.
bool reap(pid_t child, int &status) {
  const auto deadline = std::chrono::steady_clock::now() + kTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (waitpid(child, &status, WNOHANG) == child)
      return true;
    usleep(1000);
  }
  kill(child, SIGKILL);
  waitpid(child, &status, 0);
  return false;
}

// True once the logger is deciding the colors, false after kTimeout.
bool wait_for_decision() {
  const auto deadline = std::chrono::steady_clock::now() + kTimeout;
  while (!g_deciding_colors.load()) {
    if (std::chrono::steady_clock::now() >= deadline)
      return false;
    std::this_thread::yield();
  }
  return true;
}

} // namespace

int main() {
  // The logger decides without isatty() under NO_COLOR, and waitpid() cannot
  // reap a child when SIGCHLD is ignored: both may come from the environment.
  unsetenv("NO_COLOR");
  std::signal(SIGCHLD, SIG_DFL);

  std::thread decider([] { (void)coretrace::color(coretrace::Color::Red); });
  const bool deciding = wait_for_decision();
  CHECK(deciding);
  if (!deciding) {
    decider.join();
    return ct_test::result();
  }

  const pid_t child = fork();
  if (child == 0) {
    (void)coretrace::color(coretrace::Color::Red);
    _exit(0);
  }
  decider.join();
  CHECK(child > 0);
  if (child <= 0)
    return ct_test::result();

  int status = 0;
  const bool exited = reap(child, status);
  CHECK(exited);
  CHECK(exited && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  return ct_test::result();
}
