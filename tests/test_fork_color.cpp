// A child of fork() can use the colors although another thread of the parent
// was deciding them at the time of the fork: the child decides them again,
// instead of waiting for a decision that no thread of the child will ever
// finish.

#include "check.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern std::atomic<bool> g_deciding_colors;

namespace {

constexpr std::chrono::seconds kChildTimeout{5};

// True when `child` exited, with its status in `status`; a child still running
// after kChildTimeout is killed.
bool reap(pid_t child, int &status) {
  const auto deadline = std::chrono::steady_clock::now() + kChildTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (waitpid(child, &status, WNOHANG) == child)
      return true;
    usleep(1000);
  }
  kill(child, SIGKILL);
  waitpid(child, &status, 0);
  return false;
}

} // namespace

int main() {
  std::thread decider([] { (void)coretrace::color(coretrace::Color::Red); });
  while (!g_deciding_colors.load()) {
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
