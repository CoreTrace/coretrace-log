#include "check.hpp"

#include <coretrace/logger.hpp>

#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kThreads = 4;
constexpr int kLines = 2000;
constexpr int kLinesPerFormatError = 100;

} // namespace

int main() {
  using namespace coretrace;

  // Keep the output free of color codes, even when stderr is a terminal.
  CHECK(ct_test::set_env("NO_COLOR", "1"));

  set_sink(ct_test::capture_sink);
  enable_logging();
  set_thread_safe(true);

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t] {
      for (int i = 0; i < kLines; ++i) {
        log(Level::Info, "thread {} line {}\n", t, i);
        if (i % kLinesPerFormatError == 0)
          log(Level::Info, "{} {}\n", t); // Missing argument: fallback line.
      }
    });
  }
  for (auto &thread : threads)
    thread.join();
  reset_sink();

  // Every captured line must be a complete log line or the fallback message.
  const std::regex log_line(R"(\|\d+\| ==ct== \[INFO\] thread \d+ line \d+)");
  int log_lines = 0;
  int fallback_lines = 0;
  int broken_lines = 0;
  std::istringstream lines(ct_test::captured);
  for (std::string line; std::getline(lines, line);) {
    if (line == "coretrace: log format error")
      ++fallback_lines;
    else if (std::regex_match(line, log_line))
      ++log_lines;
    else
      ++broken_lines;
  }

  CHECK(broken_lines == 0);
  CHECK(log_lines == kThreads * kLines);
  CHECK(fallback_lines == kThreads * (kLines / kLinesPerFormatError));
  return ct_test::result();
}
