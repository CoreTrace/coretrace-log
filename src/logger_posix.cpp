#include "logger_platform.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

namespace coretrace::platform {

[[nodiscard]] bool stderr_supports_color() { return isatty(2) != 0; }

void write_stderr(const char *data, size_t size) {
  while (size > 0) {
    ssize_t written = write(2, data, size);
    if (written > 0) {
      data += static_cast<size_t>(written);
      size -= static_cast<size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR)
      continue;
    break;
  }
}

[[nodiscard]] int process_id() { return static_cast<int>(getpid()); }

[[nodiscard]] unsigned long long current_thread_id() {
#if defined(__APPLE__)
  uint64_t tid = 0;
  (void)pthread_threadid_np(nullptr, &tid);
  return static_cast<unsigned long long>(tid);
#elif defined(__linux__)
  return static_cast<unsigned long long>(syscall(SYS_gettid));
#else
  return reinterpret_cast<unsigned long long>(pthread_self());
#endif
}

[[nodiscard]] bool utc_timestamp(UtcTimestamp &out) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    return false;

  struct tm tm_buf;
  if (gmtime_r(&ts.tv_sec, &tm_buf) == nullptr)
    return false;

  out.year = tm_buf.tm_year + 1900;
  out.month = tm_buf.tm_mon + 1;
  out.day = tm_buf.tm_mday;
  out.hour = tm_buf.tm_hour;
  out.minute = tm_buf.tm_min;
  out.second = tm_buf.tm_sec;
  out.millisecond = static_cast<int>(ts.tv_nsec / 1000000);
  return true;
}

} // namespace coretrace::platform
