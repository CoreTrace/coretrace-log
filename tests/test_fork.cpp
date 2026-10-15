// fork() while threads of the parent use the logger: the child can log. fork()
// waits for the logger's initialization and for a line another thread is
// writing, so the child inherits neither half done; a sink can fork and its
// child still log; other fork handlers can still log; fork() still works while
// the program exits; and the child's lines carry the child's pid.

#include "check.hpp"
#include "fork_helpers.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <string>
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

std::atomic<bool> g_log_from_fork_handlers{false};

void log_from_fork_handler() {
  if (!g_log_from_fork_handlers.load())
    return;
  coretrace::log(Level::Info, "from a fork handler\n");
  coretrace::set_sink(ct_test::capture_sink);
}

// Registered by this translation unit's initialization, which runs before the
// logger's: fork() calls the prepare handler after the logger's, and the
// parent and child handlers before the logger's, while the forking thread holds
// the logger's locks.
const bool g_fork_handlers_registered =
    pthread_atfork(log_from_fork_handler, log_from_fork_handler,
                   log_from_fork_handler) == 0;

void check_logging_from_fork_handlers() {
  CHECK(g_fork_handlers_registered);
  coretrace::set_sink(ct_test::capture_sink);
  const size_t from = ct_test::captured.size();
  g_log_from_fork_handlers = true;
  alarm(5); // A fork() that hangs in a handler kills the test.
  const pid_t child = fork();
  if (child == 0)
    _exit(0);
  alarm(0);
  g_log_from_fork_handlers = false;
  CHECK(ct_test::child_succeeded(child));

  // One line from the prepare handler, one from the parent handler.
  constexpr std::string_view line = "from a fork handler";
  const size_t first = ct_test::captured.find(line, from);
  CHECK(first != std::string::npos && ct_test::logged(line, first + 1));
  coretrace::reset_sink();
}

void fork_at_exit() {
  const pid_t child = fork();
  if (child == 0)
    _exit(0);
  if (!ct_test::child_succeeded(child)) {
    std::fputs("fork() at exit failed\n", stderr);
    _exit(1);
  }
}

// Registered by this translation unit's initialization, which runs before the
// logger's: fork_at_exit() runs after the logger's static destructors.
const bool g_fork_at_exit_registered = std::atexit(fork_at_exit) == 0;

} // namespace

int main() {
  std::signal(SIGCHLD, SIG_DFL);
  CHECK(g_fork_at_exit_registered);
  check_fork_initializes();
  check_child_pid();
  coretrace::set_min_level(Level::Info);
  coretrace::enable_logging();
  check_fork_while_writing();
  check_fork_from_sink();
  check_logging_from_fork_handlers();
  return ct_test::result();
}
