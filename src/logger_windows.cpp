// SPDX-License-Identifier: MIT
#include "logger_platform.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <limits>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>

namespace coretrace::platform {

namespace {

[[nodiscard]] bool enable_virtual_terminal_on_stderr() {
  HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
    return false;

  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode))
    return false;

  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
    return true;

  return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

} // namespace

[[nodiscard]] bool stderr_supports_color() {
  if (_isatty(_fileno(stderr)) == 0)
    return false;

  return enable_virtual_terminal_on_stderr();
}

void write_stderr(const char *data, size_t size) {
  const int fd = _fileno(stderr);
  while (size > 0) {
    const size_t chunk =
        std::min(size, static_cast<size_t>((std::numeric_limits<int>::max)()));
    const int written = _write(fd, data, static_cast<unsigned int>(chunk));
    if (written > 0) {
      data += static_cast<size_t>(written);
      size -= static_cast<size_t>(written);
      continue;
    }
    break;
  }
}

[[nodiscard]] int process_id() { return static_cast<int>(GetCurrentProcessId()); }

[[nodiscard]] unsigned long long current_thread_id() {
  return static_cast<unsigned long long>(GetCurrentThreadId());
}

[[nodiscard]] bool utc_timestamp(UtcTimestamp &out) {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto time = clock::to_time_t(now);
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()) %
      1000;

  std::tm tm_buf{};
  if (gmtime_s(&tm_buf, &time) != 0)
    return false;

  out.year = tm_buf.tm_year + 1900;
  out.month = tm_buf.tm_mon + 1;
  out.day = tm_buf.tm_mday;
  out.hour = tm_buf.tm_hour;
  out.minute = tm_buf.tm_min;
  out.second = tm_buf.tm_sec;
  out.millisecond = static_cast<int>(millis.count());
  return true;
}

} // namespace coretrace::platform
