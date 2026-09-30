#include "check.hpp"

#include <coretrace/logger.hpp>

#include <atomic>
#include <cstddef>
#include <thread>

namespace {

constexpr int kIterations = 20000;

std::atomic<int> calls_in_sink{0};
std::atomic<int> overlapping_calls{0};

// Thread-safe mode must never run the sink on two threads at once.
void overlap_detecting_sink(const char *, size_t) {
  if (calls_in_sink.fetch_add(1) != 0)
    overlapping_calls.fetch_add(1);
  std::this_thread::yield();
  calls_in_sink.fetch_sub(1);
}

} // namespace

int main() {
  using namespace coretrace;

  set_sink(overlap_detecting_sink);
  enable_logging();
  set_thread_safe(true);

  std::thread valid([] {
    for (int i = 0; i < kIterations; ++i)
      log(Level::Info, "valid {}\n", i);
  });
  std::thread invalid([] {
    for (int i = 0; i < kIterations; ++i)
      log(Level::Info, "{} {}\n", i); // Missing argument: format error.
  });
  valid.join();
  invalid.join();
  reset_sink();

  CHECK(overlapping_calls.load() == 0);
  return ct_test::result();
}
