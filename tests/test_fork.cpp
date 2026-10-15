// fork() while threads of the parent use the logger: the child can log. fork()
// waits for the logger's initialization and for a line another thread is
// writing, so the child inherits neither half done; a sink can fork and its
// child still log; and the child's lines carry the child's pid.

#include "check.hpp"
#include "fork_helpers.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace {

using coretrace::Level;

// fork() finishes the logger's initialization when no thread has: the
// environment defaults are those at the time of the fork. Runs first, before
// any call initializes the logger.
void check_fork_initializes() {
  CHECK(ct_test::set_env("CT_LOG_LEVEL", "debug"));
  const pid_t child = fork();
  if (child == 0)
    _exit(coretrace::min_level() == Level::Debug ? 0 : 1);
  CHECK(ct_test::set_env("CT_LOG_LEVEL", "error"));
  CHECK(coretrace::min_level() == Level::Debug);
  CHECK(ct_test::child_succeeded(child));
}

void check_child_pid() {
  CHECK(coretrace::pid() == getpid());
  const pid_t child = fork();
  if (child == 0)
    _exit(coretrace::pid() == getpid() ? 0 : 1);
  CHECK(ct_test::child_succeeded(child));
}

std::atomic<bool> g_writing{false};

// Writes the first line in 300 ms, long enough for the test to fork meanwhile.
void slow_sink(const char *, size_t) {
  if (!g_writing.exchange(true))
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void check_fork_while_writing() {
  coretrace::set_sink(slow_sink);
  // The writer outlives the fork: ThreadSanitizer reports a thread that a
  // child inherits finished and not joined.
  std::atomic<bool> forked{false};
  std::thread writer([&] {
    coretrace::log(Level::Info, "parent\n");
    (void)ct_test::wait_for(forked);
  });
  const bool writing = ct_test::wait_for(g_writing);
  CHECK(writing);
  const pid_t child = writing ? fork() : -1;
  if (child == 0) {
    coretrace::log(Level::Info, "child\n");
    _exit(0);
  }
  forked = true;
  writer.join();
  CHECK(ct_test::child_succeeded(child));
  coretrace::reset_sink();
}

pid_t g_sink_child = -1;

// Forks on the line "fork": the child logs, from the sink and after it.
void forking_sink(const char *data, size_t size) {
  if (std::string_view(data, size).find("fork") == std::string_view::npos)
    return;
  g_sink_child = fork();
  if (g_sink_child == 0)
    coretrace::log(Level::Info, "child, in the sink\n");
}

void check_fork_from_sink() {
  coretrace::set_sink(forking_sink);
  coretrace::log(Level::Info, "fork\n");
  if (g_sink_child == 0) {
    coretrace::log(Level::Info, "child, after the sink\n");
    _exit(0);
  }
  coretrace::reset_sink();
  CHECK(ct_test::child_succeeded(g_sink_child));
}

} // namespace

int main() {
  std::signal(SIGCHLD, SIG_DFL);
  check_fork_initializes();
  check_child_pid();
  coretrace::set_min_level(Level::Info);
  coretrace::enable_logging();
  check_fork_while_writing();
  check_fork_from_sink();
  return ct_test::result();
}
