// SPDX-License-Identifier: Apache-2.0
#include "coretrace/logger.hpp"

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <format>
#include <mutex>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>

namespace coretrace {

namespace {

std::atomic<bool> g_log_enabled{false};
std::atomic<int> g_min_level{static_cast<int>(Level::Info)};
std::atomic<bool> g_thread_safe{true};
std::atomic<bool> g_timestamps_enabled{false};
std::atomic<bool> g_source_location_enabled{false};
std::atomic<SinkFn> g_sink{nullptr};
std::atomic<bool> g_min_level_set_explicitly{false};
std::atomic<bool> g_modules_set_explicitly{false};

std::mutex g_state_mutex;
std::mutex g_output_mutex;
std::once_flag g_init_once;

std::string g_prefix = "==ct==";

constexpr int kMaxModules = 32;
std::string g_modules[kMaxModules];
int g_module_count = 0;
bool g_module_filter_active = false;

[[nodiscard]] bool ascii_case_equal(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    const char left =
        static_cast<char>(std::tolower(static_cast<unsigned char>(lhs[i])));
    const char right =
        static_cast<char>(std::tolower(static_cast<unsigned char>(rhs[i])));
    if (left != right) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] bool enable_virtual_terminal_on_stderr() {
  HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
  if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
    return false;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(handle, &mode)) {
    return false;
  }

  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
    return true;
  }

  return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

[[nodiscard]] bool use_color() {
  static const bool enabled = []() {
    if (std::getenv("NO_COLOR") != nullptr) {
      return false;
    }

    if (_isatty(_fileno(stderr)) == 0) {
      return false;
    }

    return enable_virtual_terminal_on_stderr();
  }();

  return enabled;
}

[[nodiscard]] std::string prefix_snapshot() {
  std::lock_guard<std::mutex> lock(g_state_mutex);
  return g_prefix;
}

void add_module_locked(std::string_view name) {
  for (int i = 0; i < g_module_count; ++i) {
    if (g_modules[i] == name) {
      return;
    }
  }

  if (g_module_count >= kMaxModules) {
    return;
  }

  g_modules[g_module_count++] = std::string(name);
  g_module_filter_active = true;
}

[[nodiscard]] int parse_level_from_env(const char *value) {
  if (value == nullptr) {
    return static_cast<int>(Level::Info);
  }
  if (ascii_case_equal(value, "debug")) {
    return static_cast<int>(Level::Debug);
  }
  if (ascii_case_equal(value, "warn")) {
    return static_cast<int>(Level::Warn);
  }
  if (ascii_case_equal(value, "error")) {
    return static_cast<int>(Level::Error);
  }
  return static_cast<int>(Level::Info);
}

void init_from_env() {
  if (!g_min_level_set_explicitly.load(std::memory_order_acquire)) {
    g_min_level.store(parse_level_from_env(std::getenv("CT_LOG_LEVEL")),
                      std::memory_order_release);
  }

  if (!g_modules_set_explicitly.load(std::memory_order_acquire)) {
    const char *env_debug = std::getenv("CT_DEBUG");
    if (env_debug == nullptr || *env_debug == '\0') {
      return;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    const char *start = env_debug;
    while (*start != '\0') {
      const char *end = start;
      while (*end != '\0' && *end != ',') {
        ++end;
      }
      if (end > start) {
        add_module_locked(
            std::string_view(start, static_cast<size_t>(end - start)));
      }
      start = (*end == ',') ? end + 1 : end;
    }
  }
}

[[nodiscard]] std::string make_timestamp_prefix() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto time = clock::to_time_t(now);
  const auto millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()) %
      1000;

  std::tm tm_buf{};
  gmtime_s(&tm_buf, &time);

  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02dT%02d:%02d:%02d.%03d] ",
                tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
                static_cast<int>(millis.count()));
  return buffer;
}

[[nodiscard]] const char *basename_of(const char *path) {
  if (path == nullptr) {
    return "<unknown>";
  }

  const char *last = path;
  for (const char *p = path; *p != '\0'; ++p) {
    if (*p == '/' || *p == '\\') {
      last = p + 1;
    }
  }
  return last;
}

} // namespace

void enable_logging() { g_log_enabled.store(true, std::memory_order_release); }

void disable_logging() { g_log_enabled.store(false, std::memory_order_release); }

[[nodiscard]] bool log_is_enabled() {
  return g_log_enabled.load(std::memory_order_acquire);
}

void set_prefix(std::string_view prefix) {
  std::lock_guard<std::mutex> lock(g_state_mutex);
  g_prefix.assign(prefix);
}

void set_min_level(Level level) {
  g_min_level_set_explicitly.store(true, std::memory_order_release);
  init_once();
  g_min_level.store(static_cast<int>(level), std::memory_order_release);
}

[[nodiscard]] Level min_level() {
  return static_cast<Level>(g_min_level.load(std::memory_order_acquire));
}

void enable_module(std::string_view name) {
  if (name.empty()) {
    return;
  }

  g_modules_set_explicitly.store(true, std::memory_order_release);
  init_once();

  std::lock_guard<std::mutex> lock(g_state_mutex);
  add_module_locked(name);
}

void disable_module(std::string_view name) {
  if (name.empty()) {
    return;
  }

  g_modules_set_explicitly.store(true, std::memory_order_release);
  init_once();

  std::lock_guard<std::mutex> lock(g_state_mutex);
  for (int i = 0; i < g_module_count; ++i) {
    if (g_modules[i] == name) {
      for (int j = i; j < g_module_count - 1; ++j) {
        g_modules[j] = g_modules[j + 1];
      }
      --g_module_count;
      if (g_module_count == 0) {
        g_module_filter_active = false;
      }
      break;
    }
  }
}

void enable_all_modules() {
  g_modules_set_explicitly.store(true, std::memory_order_release);
  init_once();

  std::lock_guard<std::mutex> lock(g_state_mutex);
  g_module_count = 0;
  g_module_filter_active = false;
}

[[nodiscard]] bool module_is_enabled(std::string_view name) {
  std::lock_guard<std::mutex> lock(g_state_mutex);
  if (!g_module_filter_active) {
    return true;
  }

  for (int i = 0; i < g_module_count; ++i) {
    if (g_modules[i] == name) {
      return true;
    }
  }

  return false;
}

void set_thread_safe(bool enabled) {
  g_thread_safe.store(enabled, std::memory_order_release);
}

void set_sink(SinkFn fn) { g_sink.store(fn, std::memory_order_release); }

void reset_sink() { g_sink.store(nullptr, std::memory_order_release); }

void set_timestamps(bool enabled) {
  g_timestamps_enabled.store(enabled, std::memory_order_release);
}

void set_source_location(bool enabled) {
  g_source_location_enabled.store(enabled, std::memory_order_release);
}

[[nodiscard]] std::string_view color(Color c) {
  if (!use_color()) {
    return {};
  }

  switch (c) {
  case Color::Reset:
    return "\x1b[0m";
  case Color::Dim:
    return "\x1b[2m";
  case Color::Bold:
    return "\x1b[1m";
  case Color::Underline:
    return "\x1b[4m";
  case Color::Italic:
    return "\x1b[3m";
  case Color::Blink:
    return "\x1b[5m";
  case Color::Reverse:
    return "\x1b[7m";
  case Color::Hidden:
    return "\x1b[8m";
  case Color::Strike:
    return "\x1b[9m";
  case Color::Black:
    return "\x1b[30m";
  case Color::Red:
    return "\x1b[31m";
  case Color::Green:
    return "\x1b[32m";
  case Color::Yellow:
    return "\x1b[33m";
  case Color::Blue:
    return "\x1b[34m";
  case Color::Magenta:
    return "\x1b[35m";
  case Color::Cyan:
    return "\x1b[36m";
  case Color::White:
    return "\x1b[37m";
  case Color::Gray:
    return "\x1b[90m";
  case Color::BrightRed:
    return "\x1b[91m";
  case Color::BrightGreen:
    return "\x1b[92m";
  case Color::BrightYellow:
    return "\x1b[93m";
  case Color::BrightBlue:
    return "\x1b[94m";
  case Color::BrightMagenta:
    return "\x1b[95m";
  case Color::BrightCyan:
    return "\x1b[96m";
  case Color::BrightWhite:
    return "\x1b[97m";
  case Color::BgBlack:
    return "\x1b[40m";
  case Color::BgRed:
    return "\x1b[41m";
  case Color::BgGreen:
    return "\x1b[42m";
  case Color::BgYellow:
    return "\x1b[43m";
  case Color::BgBlue:
    return "\x1b[44m";
  case Color::BgMagenta:
    return "\x1b[45m";
  case Color::BgCyan:
    return "\x1b[46m";
  case Color::BgWhite:
    return "\x1b[47m";
  case Color::BgGray:
    return "\x1b[100m";
  case Color::BgBrightRed:
    return "\x1b[101m";
  case Color::BgBrightGreen:
    return "\x1b[102m";
  case Color::BgBrightYellow:
    return "\x1b[103m";
  case Color::BgBrightBlue:
    return "\x1b[104m";
  case Color::BgBrightMagenta:
    return "\x1b[105m";
  case Color::BgBrightCyan:
    return "\x1b[106m";
  case Color::BgBrightWhite:
    return "\x1b[107m";
  }

  return {};
}

[[nodiscard]] std::string_view level_label(Level level) {
  switch (level) {
  case Level::Debug:
    return "DEBUG";
  case Level::Info:
    return "INFO";
  case Level::Warn:
    return "WARN";
  case Level::Error:
    return "ERROR";
  }
  return "INFO";
}

[[nodiscard]] std::string_view level_color(Level level) {
  switch (level) {
  case Level::Debug:
    return color(Color::Cyan);
  case Level::Info:
    return color(Color::Green);
  case Level::Warn:
    return color(Color::Yellow);
  case Level::Error:
    return color(Color::Red);
  }
  return color(Color::Cyan);
}

void write_raw(const char *data, size_t size) {
  if (data == nullptr || size == 0) {
    return;
  }

  SinkFn sink = g_sink.load(std::memory_order_acquire);
  if (sink != nullptr) {
    sink(data, size);
    return;
  }

  (void)std::fwrite(data, 1, size, stderr);
  (void)std::fflush(stderr);
}

void write_str(std::string_view value) { write_raw(value.data(), value.size()); }

void write_dec(size_t value) { write_str(std::to_string(value)); }

void write_hex(uintptr_t value) { write_str(std::format("0x{:x}", value)); }

[[nodiscard]] int pid() { return static_cast<int>(GetCurrentProcessId()); }

[[nodiscard]] unsigned long long thread_id() {
  return static_cast<unsigned long long>(GetCurrentThreadId());
}

void write_prefix(Level level) {
  const std::string prefix = prefix_snapshot();

  write_str(color(Color::Dim));
  write_str(std::format("|{}|", pid()));
  write_str(color(Color::Reset));
  write_raw(" ", 1);
  write_str(color(Color::Gray));
  write_str(color(Color::Italic));
  write_str(prefix);
  write_raw(" ", 1);
  write_str(color(Color::Reset));
  write_str(level_color(level));
  write_str("[");
  write_str(level_label(level));
  write_str("]");
  write_str(color(Color::Reset));
  write_raw(" ", 1);
}

void init_once() { std::call_once(g_init_once, init_from_env); }

void write_log_line(Level level, std::string_view module,
                    std::string_view message, const std::source_location &loc) {
  std::unique_lock<std::mutex> lock(g_output_mutex, std::defer_lock);
  if (g_thread_safe.load(std::memory_order_acquire)) {
    lock.lock();
  }

  std::string line;
  if (g_timestamps_enabled.load(std::memory_order_acquire)) {
    line += make_timestamp_prefix();
  }

  line += color(Color::Dim);
  line += std::format("|{}|", pid());
  line += color(Color::Reset);
  line += " ";
  line += color(Color::Gray);
  line += color(Color::Italic);
  line += prefix_snapshot();
  line += " ";
  line += color(Color::Reset);
  line += level_color(level);
  line += "[";
  line += level_label(level);
  line += "]";
  line += color(Color::Reset);

  if (g_source_location_enabled.load(std::memory_order_acquire)) {
    line += " ";
    line += color(Color::Dim);
    line += basename_of(loc.file_name());
    line += ":";
    line += std::to_string(loc.line());
    line += color(Color::Reset);
  }

  if (!module.empty()) {
    line += " ";
    line += color(Color::Dim);
    line += "(";
    line.append(module.data(), module.size());
    line += ")";
    line += color(Color::Reset);
  }

  line += " ";
  line.append(message.data(), message.size());
  write_raw(line.data(), line.size());
}

} // namespace coretrace
