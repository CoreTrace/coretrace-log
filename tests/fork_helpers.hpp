#ifndef CORETRACE_LOGGER_TESTS_FORK_HELPERS_HPP
#define CORETRACE_LOGGER_TESTS_FORK_HELPERS_HPP

// Helpers shared by the POSIX tests that fork. They need the default SIGCHLD
// action, which each test sets: when SIGCHLD is ignored, waitpid() cannot reap
// a child.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace ct_test {

inline constexpr std::chrono::seconds kForkTimeout{5};

/// True when `child` exits with status 0. A child still running after
/// kForkTimeout is killed, and reported as hung.
[[nodiscard]] inline bool child_succeeded(pid_t child) {
  if (child <= 0)
    return false;
  int status = 0;
  const auto deadline = std::chrono::steady_clock::now() + kForkTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (waitpid(child, &status, WNOHANG) == child)
      return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    usleep(1000);
  }
  kill(child, SIGKILL);
  waitpid(child, &status, 0);
  std::fprintf(stderr, "child %d hung: killed after %lld s\n",
               static_cast<int>(child),
               static_cast<long long>(kForkTimeout.count()));
  return false;
}

/// True once `flag` is set, false after kForkTimeout.
[[nodiscard]] inline bool wait_for(const std::atomic<bool> &flag) {
  const auto deadline = std::chrono::steady_clock::now() + kForkTimeout;
  while (!flag.load()) {
    if (std::chrono::steady_clock::now() >= deadline)
      return false;
    std::this_thread::yield();
  }
  return true;
}

} // namespace ct_test

#endif // CORETRACE_LOGGER_TESTS_FORK_HELPERS_HPP
