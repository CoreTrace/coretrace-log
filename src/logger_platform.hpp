#ifndef CORETRACE_LOGGER_PLATFORM_HPP
#define CORETRACE_LOGGER_PLATFORM_HPP

#include <cstddef>

namespace coretrace::platform {

struct UtcTimestamp {
  int year = 1970;
  int month = 1;
  int day = 1;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int millisecond = 0;
};

[[nodiscard]] bool stderr_supports_color();
void write_stderr(const char *data, size_t size);
[[nodiscard]] int process_id();
[[nodiscard]] unsigned long long current_thread_id();
[[nodiscard]] bool utc_timestamp(UtcTimestamp &out);

} // namespace coretrace::platform

#endif // CORETRACE_LOGGER_PLATFORM_HPP
