#include "check.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>

namespace {

std::atomic<bool> old_sink_retired{false};
std::atomic<int> late_old_sink_calls{0};

// Slow like a blocking fwrite(). Counts calls still running after the caller
// replaced this sink and may have released its resources.
void old_sink(const char *, size_t) {
  std::this_thread::sleep_for(std::chrono::microseconds(200));
  if (old_sink_retired.load())
    late_old_sink_calls.fetch_add(1);
}

void new_sink(const char *, size_t) {}

} // namespace

int main() {
  using namespace coretrace;

  set_sink(old_sink);
  enable_logging();
  set_thread_safe(true);

  std::atomic<bool> stop{false};
  std::thread logger([&stop] {
    while (!stop.load())
      log(Level::Info, "line\n");
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  set_sink(new_sink);
  // A caller would now close the file used by the old sink.
  old_sink_retired.store(true);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  stop.store(true);
  logger.join();
  reset_sink();

  CHECK(late_old_sink_calls.load() == 0);
  return ct_test::result();
}
