#include "coretrace/logger.hpp"

#include "logger_platform.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string_view>

namespace coretrace {

namespace {

// ── Shared limits ─────────────────────────

constexpr int MAX_MODULES = 32;
constexpr int MODULE_NAME_LEN = 32;

// ── Enable / Disable ─────────────────────

std::atomic<int> g_log_enabled{0};

// ── Prefix ───────────────────────────────

char g_prefix_buf[64] = "==ct==";
size_t g_prefix_len = 6;

// ── Level filtering ──────────────────────

std::atomic<int> g_min_level{static_cast<int>(Level::Info)};
std::atomic<int> g_min_level_set_explicitly{0};

// ── Module filtering ─────────────────────

struct ModuleTable {
  char names[MAX_MODULES][MODULE_NAME_LEN];
  int count = 0;
  int filter_active = 0; // 1 if at least one module was registered
};

ModuleTable g_modules{};
std::atomic<int> g_modules_set_explicitly{0};

// ── Synchronization ──────────────────────

// Protects mutable logger state (prefix + modules table).
std::mutex g_state_mutex;

// Protects atomicity of one log line output when thread-safe mode is on.
std::mutex g_output_mutex;
std::atomic<int> g_thread_safe{1}; // enabled by default

// ── Sink ─────────────────────────────────

std::atomic<SinkFn> g_sink{nullptr};

// ── Timestamps ───────────────────────────

std::atomic<int> g_timestamps_enabled{0};

// ── Source location ──────────────────────

std::atomic<int> g_source_location_enabled{0};

// ── Init ─────────────────────────────────

std::once_flag g_init_once;

// ── Small lock guards ────────────────────

struct StateLockGuard {
  StateLockGuard() { g_state_mutex.lock(); }
  ~StateLockGuard() { g_state_mutex.unlock(); }

  StateLockGuard(const StateLockGuard &) = delete;
  StateLockGuard &operator=(const StateLockGuard &) = delete;
};

struct OutputLockGuard {
  OutputLockGuard()
      : locked(g_thread_safe.load(std::memory_order_acquire) != 0) {
    if (locked)
      g_output_mutex.lock();
  }

  ~OutputLockGuard() {
    if (locked)
      g_output_mutex.unlock();
  }

  OutputLockGuard(const OutputLockGuard &) = delete;
  OutputLockGuard &operator=(const OutputLockGuard &) = delete;

  bool locked;
};

struct PrefixSnapshot {
  char value[sizeof(g_prefix_buf)];
  size_t len = 0;
};

[[nodiscard]] PrefixSnapshot read_prefix_snapshot() {
  PrefixSnapshot snapshot{};

  StateLockGuard guard;

  snapshot.len = g_prefix_len;
  if (snapshot.len > sizeof(snapshot.value))
    snapshot.len = sizeof(snapshot.value);

  std::memcpy(snapshot.value, g_prefix_buf, snapshot.len);
  return snapshot;
}

// ── Environment ───────────────────────────

[[nodiscard]] const char *env_var(const char *name) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
  const char *value = std::getenv(name);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  return value;
}

// ── Color detection ──────────────────────

[[nodiscard]] bool use_color() {
  static const bool enabled = []() {
    if (env_var("NO_COLOR") != nullptr)
      return false;

    return platform::stderr_supports_color();
  }();

  return enabled;
}

// ── String helpers ───────────────────────

[[nodiscard]] bool sv_eq(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

[[nodiscard]] bool cstr_ieq(const char *a, const char *b) {
  while (*a && *b) {
    char ca = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
    char cb = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
    if (ca != cb)
      return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

[[nodiscard]] int parse_level_from_env(const char *value) {
  if (!value)
    return static_cast<int>(Level::Info);
  if (cstr_ieq(value, "debug"))
    return static_cast<int>(Level::Debug);
  if (cstr_ieq(value, "warn"))
    return static_cast<int>(Level::Warn);
  if (cstr_ieq(value, "error"))
    return static_cast<int>(Level::Error);
  return static_cast<int>(Level::Info);
}

// ── Timestamp formatting ─────────────────

// Writes ISO 8601 timestamp: [2025-01-15T10:45:23.456]
// Uses stack buffer, no heap allocation.
void write_timestamp_to(char *buf, size_t &idx) {
  platform::UtcTimestamp ts{};
  if (!platform::utc_timestamp(ts))
    return;

  // [YYYY-MM-DDThh:mm:ss.mmm]
  buf[idx++] = '[';

  // Year
  buf[idx++] = static_cast<char>('0' + (ts.year / 1000));
  buf[idx++] = static_cast<char>('0' + (ts.year / 100) % 10);
  buf[idx++] = static_cast<char>('0' + (ts.year / 10) % 10);
  buf[idx++] = static_cast<char>('0' + ts.year % 10);
  buf[idx++] = '-';

  // Month
  buf[idx++] = static_cast<char>('0' + ts.month / 10);
  buf[idx++] = static_cast<char>('0' + ts.month % 10);
  buf[idx++] = '-';

  // Day
  buf[idx++] = static_cast<char>('0' + ts.day / 10);
  buf[idx++] = static_cast<char>('0' + ts.day % 10);
  buf[idx++] = 'T';

  // Hour
  buf[idx++] = static_cast<char>('0' + ts.hour / 10);
  buf[idx++] = static_cast<char>('0' + ts.hour % 10);
  buf[idx++] = ':';

  // Minute
  buf[idx++] = static_cast<char>('0' + ts.minute / 10);
  buf[idx++] = static_cast<char>('0' + ts.minute % 10);
  buf[idx++] = ':';

  // Second
  buf[idx++] = static_cast<char>('0' + ts.second / 10);
  buf[idx++] = static_cast<char>('0' + ts.second % 10);
  buf[idx++] = '.';

  // Milliseconds
  buf[idx++] = static_cast<char>('0' + ts.millisecond / 100);
  buf[idx++] = static_cast<char>('0' + (ts.millisecond / 10) % 10);
  buf[idx++] = static_cast<char>('0' + ts.millisecond % 10);

  buf[idx++] = ']';
  buf[idx++] = ' ';
}

// ── Extract basename from path ───────────

[[nodiscard]] const char *basename_of(const char *path) {
  if (!path)
    return "<unknown>";

  const char *last = path;
  for (const char *p = path; *p; ++p) {
    if (*p == '/' || *p == '\\')
      last = p + 1;
  }
  return last;
}

// ── Module helpers (state lock required) ─

void add_module_locked(std::string_view name) {
  // Check if already registered.
  for (int i = 0; i < g_modules.count; ++i) {
    if (sv_eq(name, std::string_view(g_modules.names[i])))
      return;
  }

  if (g_modules.count < MAX_MODULES) {
    for (size_t i = 0; i < name.size(); ++i)
      g_modules.names[g_modules.count][i] = name[i];
    g_modules.names[g_modules.count][name.size()] = '\0';
    g_modules.count++;
    g_modules.filter_active = 1;
  }
}

void init_from_env() {
  // CT_LOG_LEVEL=debug|info|warn|error
  // (startup default only, explicit API has priority)
  if (g_min_level_set_explicitly.load(std::memory_order_acquire) == 0) {
    const char *env_level = env_var("CT_LOG_LEVEL");
    if (env_level)
      g_min_level.store(parse_level_from_env(env_level),
                        std::memory_order_release);
  }

  // CT_DEBUG=mod1,mod2,... (default only, explicit API has priority)
  if (g_modules_set_explicitly.load(std::memory_order_acquire) == 0) {
    const char *env_debug = env_var("CT_DEBUG");
    if (env_debug && env_debug[0] != '\0') {
      StateLockGuard guard;

      // Parse comma-separated module names.
      const char *start = env_debug;
      while (*start) {
        const char *end = start;
        while (*end && *end != ',')
          ++end;

        size_t len = static_cast<size_t>(end - start);
        if (len > 0 && len < MODULE_NAME_LEN)
          add_module_locked(std::string_view(start, len));

        start = *end ? end + 1 : end;
      }
    }
  }
}

} // namespace

// ####################################
//  Init
// ####################################

void init_once() { std::call_once(g_init_once, init_from_env); }

// ####################################
//  Enable / Disable
// ####################################

void enable_logging() { g_log_enabled.store(1, std::memory_order_release); }

void disable_logging() { g_log_enabled.store(0, std::memory_order_release); }

[[nodiscard]] bool log_is_enabled() {
  return g_log_enabled.load(std::memory_order_acquire) != 0;
}

// ####################################
//  Prefix
// ####################################

void set_prefix(std::string_view prefix) {
  size_t len = prefix.size();
  if (len >= sizeof(g_prefix_buf))
    len = sizeof(g_prefix_buf) - 1;

  StateLockGuard guard;

  for (size_t i = 0; i < len; ++i)
    g_prefix_buf[i] = prefix[i];
  g_prefix_buf[len] = '\0';

  g_prefix_len = len;
}

// ####################################
//  Level filtering
// ####################################

void set_min_level(Level level) {
  g_min_level_set_explicitly.store(1, std::memory_order_release);
  init_once();
  g_min_level.store(static_cast<int>(level), std::memory_order_release);
}

[[nodiscard]] Level min_level() {
  return static_cast<Level>(g_min_level.load(std::memory_order_acquire));
}

// ####################################
//  Module filtering
// ####################################

void enable_module(std::string_view name) {
  if (name.empty() || name.size() >= MODULE_NAME_LEN)
    return;

  g_modules_set_explicitly.store(1, std::memory_order_release);
  init_once();

  StateLockGuard guard;
  add_module_locked(name);
}

void disable_module(std::string_view name) {
  if (name.empty())
    return;

  g_modules_set_explicitly.store(1, std::memory_order_release);
  init_once();

  StateLockGuard guard;

  for (int i = 0; i < g_modules.count; ++i) {
    if (sv_eq(name, std::string_view(g_modules.names[i]))) {
      // Shift remaining entries.
      for (int j = i; j < g_modules.count - 1; ++j)
        std::memcpy(g_modules.names[j], g_modules.names[j + 1],
                    MODULE_NAME_LEN);
      g_modules.count--;

      if (g_modules.count == 0)
        g_modules.filter_active = 0;

      break;
    }
  }
}

void enable_all_modules() {
  g_modules_set_explicitly.store(1, std::memory_order_release);
  init_once();

  StateLockGuard guard;
  g_modules.count = 0;
  g_modules.filter_active = 0;
}

[[nodiscard]] bool module_is_enabled(std::string_view name) {
  StateLockGuard guard;

  // If no filter is active, everything passes.
  if (!g_modules.filter_active)
    return true;

  for (int i = 0; i < g_modules.count; ++i) {
    if (sv_eq(name, std::string_view(g_modules.names[i])))
      return true;
  }

  return false;
}

// ####################################
//  Thread safety
// ####################################

void set_thread_safe(bool enabled) {
  g_thread_safe.store(enabled ? 1 : 0, std::memory_order_release);
}

// ####################################
//  Sink
// ####################################

void set_sink(SinkFn fn) { g_sink.store(fn, std::memory_order_release); }

void reset_sink() { g_sink.store(nullptr, std::memory_order_release); }

// ####################################
//  Timestamps
// ####################################

void set_timestamps(bool enabled) {
  g_timestamps_enabled.store(enabled ? 1 : 0, std::memory_order_release);
}

// ####################################
//  Source location
// ####################################

void set_source_location(bool enabled) {
  g_source_location_enabled.store(enabled ? 1 : 0, std::memory_order_release);
}

// ####################################
//  Color
// ####################################

[[nodiscard]] std::string_view color(Color c) {
  if (!use_color())
    return {};

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

// ####################################
//  System info
// ####################################

[[nodiscard]] int pid() {
  static const int cached = platform::process_id();
  return cached;
}

[[nodiscard]] unsigned long long thread_id() {
  return platform::current_thread_id();
}

// ####################################
//  Low-level write
// ####################################

void write_raw(const char *data, size_t size) {
  if (!data || size == 0)
    return;

  // Custom sink?
  SinkFn sink = g_sink.load(std::memory_order_acquire);
  if (sink) {
    sink(data, size);
    return;
  }

  platform::write_stderr(data, size);
}

void write_str(std::string_view value) {
  if (value.empty())
    return;
  write_raw(value.data(), value.size());
}

void write_dec(size_t value) {
  char buf[32];
  size_t idx = 0;

  if (value == 0) {
    buf[idx++] = '0';
  } else {
    while (value != 0 && idx < sizeof(buf)) {
      buf[idx++] = static_cast<char>('0' + (value % 10));
      value /= 10;
    }
  }

  // Reverse digits.
  for (size_t i = 0; i < idx / 2; ++i) {
    char tmp = buf[i];
    buf[i] = buf[idx - 1 - i];
    buf[idx - 1 - i] = tmp;
  }

  write_raw(buf, idx);
}

void write_hex(uintptr_t value) {
  char buf[2 + sizeof(uintptr_t) * 2];
  size_t idx = 0;

  buf[idx++] = '0';
  buf[idx++] = 'x';

  bool started = false;
  for (int shift = static_cast<int>(sizeof(uintptr_t) * 8 - 4); shift >= 0;
       shift -= 4) {
    unsigned int nibble = static_cast<unsigned int>((value >> shift) & 0xF);
    if (!started && nibble == 0 && shift != 0)
      continue;

    started = true;
    if (nibble < 10)
      buf[idx++] = static_cast<char>('0' + nibble);
    else
      buf[idx++] = static_cast<char>('a' + (nibble - 10));
  }

  if (!started)
    buf[idx++] = '0';

  write_raw(buf, idx);
}

// ####################################
//  Write prefix (non-mutex, for low-level use)
// ####################################

void write_prefix(Level level) {
  PrefixSnapshot prefix = read_prefix_snapshot();

  // |PID|
  write_str(color(Color::Dim));
  write_raw("|", 1);
  write_dec(static_cast<size_t>(pid()));
  write_raw("|", 1);
  write_str(color(Color::Reset));
  write_raw(" ", 1);

  // Configurable prefix tag.
  write_str(color(Color::Gray));
  write_str(color(Color::Italic));
  write_raw(prefix.value, prefix.len);
  write_raw(" ", 1);
  write_str(color(Color::Reset));

  // [LEVEL]
  write_str(level_color(level));
  write_raw("[", 1);
  write_str(level_label(level));
  write_raw("]", 1);
  write_str(color(Color::Reset));
  write_raw(" ", 1);
}

// ####################################
//  Atomic log line output
// ####################################

void write_log_line(Level level, std::string_view module,
                    std::string_view message, const std::source_location &loc) {
  PrefixSnapshot prefix = read_prefix_snapshot();
  OutputLockGuard output_lock;

  // Optional timestamp: [2025-01-15T10:45:23.456]
  if (g_timestamps_enabled.load(std::memory_order_acquire)) {
    char ts_buf[32];
    size_t ts_idx = 0;
    write_timestamp_to(ts_buf, ts_idx);
    write_raw(ts_buf, ts_idx);
  }

  // |PID|
  write_str(color(Color::Dim));
  write_raw("|", 1);
  write_dec(static_cast<size_t>(pid()));
  write_raw("|", 1);
  write_str(color(Color::Reset));
  write_raw(" ", 1);

  // Configurable prefix tag.
  write_str(color(Color::Gray));
  write_str(color(Color::Italic));
  write_raw(prefix.value, prefix.len);
  write_raw(" ", 1);
  write_str(color(Color::Reset));

  // [LEVEL]
  write_str(level_color(level));
  write_raw("[", 1);
  write_str(level_label(level));
  write_raw("]", 1);
  write_str(color(Color::Reset));

  // Optional source location: file.cpp:42
  if (g_source_location_enabled.load(std::memory_order_acquire)) {
    write_raw(" ", 1);
    write_str(color(Color::Dim));
    const char *file = basename_of(loc.file_name());
    write_raw(file, std::strlen(file));
    write_raw(":", 1);
    write_dec(static_cast<size_t>(loc.line()));
    write_str(color(Color::Reset));
  }

  // Optional module tag: (alloc)
  if (!module.empty()) {
    write_raw(" ", 1);
    write_str(color(Color::Dim));
    write_raw("(", 1);
    write_raw(module.data(), module.size());
    write_raw(")", 1);
    write_str(color(Color::Reset));
  }

  write_raw(" ", 1);

  // Message body.
  write_raw(message.data(), message.size());
}

} // namespace coretrace
