#ifndef CORETRACE_LOGGER_HPP
#define CORETRACE_LOGGER_HPP

#include <cstddef>
#include <cstdint>
#include <format>
#include <source_location>
#include <string>
#include <string_view>

namespace coretrace {

// #######################################
//  Color
// #######################################

enum class Color {
  Reset,

  Dim,
  Bold,
  Underline,
  Italic,
  Blink,
  Reverse,
  Hidden,
  Strike,

  Black,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,

  Gray,
  BrightRed,
  BrightGreen,
  BrightYellow,
  BrightBlue,
  BrightMagenta,
  BrightCyan,
  BrightWhite,

  BgBlack,
  BgRed,
  BgGreen,
  BgYellow,
  BgBlue,
  BgMagenta,
  BgCyan,
  BgWhite,

  BgGray,
  BgBrightRed,
  BgBrightGreen,
  BgBrightYellow,
  BgBrightBlue,
  BgBrightMagenta,
  BgBrightCyan,
  BgBrightWhite,
};

// #######################################
//  Level
// #######################################

enum class Level { Info = 0, Warn = 1, Error = 2 };

// #######################################
//  LogEntry — captures Level + source_location
// #######################################

/// Implicitly convertible from Level so that the caller can write:
///   coretrace::log(Level::Info, "msg {}\n", val);
/// and the source_location is captured at the call site.
struct LogEntry {
  Level level;
  std::source_location loc;

  // NOLINTNEXTLINE(google-explicit-constructor)
  LogEntry(Level l, std::source_location loc = std::source_location::current())
      : level(l), loc(loc) {}
};

// #######################################
//  Module — strong type for module names
// #######################################

/// Wraps a module name to disambiguate the log() overload.
///
/// Example:
///   coretrace::log(Level::Info, Module("alloc"), "malloc size={}\n", 64);
///
struct Module {
  std::string_view name;

  explicit Module(std::string_view n) : name(n) {}
  explicit Module(const char *n) : name(n) {}
};

// #######################################
//  Core API
// #######################################

/// Enable logging output (disabled by default).
void enable_logging();

/// Disable logging output.
void disable_logging();

/// Check whether logging is currently enabled.
[[nodiscard]] bool log_is_enabled();

/// Set the log prefix tag (default: "==ct==").
/// Thread-safe. The string is copied internally.
void set_prefix(std::string_view prefix);

// #######################################
//  Level filtering
// #######################################

/// Set the minimum log level. Messages below this level are silently dropped.
/// Default: Level::Info (everything passes).
/// Env var CT_LOG_LEVEL=info|warn|error is used as a startup default only.
/// Explicit API calls always take precedence.
void set_min_level(Level level);

/// Return the current minimum log level.
[[nodiscard]] Level min_level();

// #######################################
//  Module filtering
// #######################################

/// Enable a named module for logging. When at least one module is enabled,
/// only log() calls that specify an enabled module will produce output.
/// Module names are case-sensitive and stored in a fixed-size table.
/// Env var CT_DEBUG=mod1,mod2,... is used as a startup default only.
/// Explicit API calls always take precedence.
void enable_module(std::string_view name);

/// Disable a previously enabled module.
void disable_module(std::string_view name);

/// Clear the module filter so that all log() calls pass again.
void enable_all_modules();

/// Check whether a module is currently enabled (or no filter is active).
[[nodiscard]] bool module_is_enabled(std::string_view name);

// #######################################
//  Thread safety
// #######################################

/// Enable or disable mutex-based serialization of log output.
/// Default: true (thread-safe). Set to false for single-threaded hot paths.
void set_thread_safe(bool enabled);

// #######################################
//  Sink (output destination)
// #######################################

/// Callback type for custom sinks.
using SinkFn = void (*)(const char *data, size_t size);

/// Redirect all log output to a custom sink function.
/// Pass nullptr to revert to stderr (same as reset_sink()).
void set_sink(SinkFn fn);

/// Revert to the default stderr sink.
void reset_sink();

// #######################################
//  Timestamps
// #######################################

/// Enable or disable ISO 8601 timestamps in the log prefix.
/// Default: false.
void set_timestamps(bool enabled);

// #######################################
//  Source location
// #######################################

/// Enable or disable file:line display in the log prefix.
/// Default: false.
void set_source_location(bool enabled);

// #######################################
//  Color helpers
// #######################################

/// Return the ANSI escape sequence for the given color.
/// Returns empty string_view when color output is disabled.
[[nodiscard]] std::string_view color(Color c);

/// Return the label string for a log level ("INFO", "WARN", "ERROR").
[[nodiscard]] std::string_view level_label(Level level);

/// Return the color escape sequence for a log level.
[[nodiscard]] std::string_view level_color(Level level);

// #######################################
//  Low-level write
// #######################################

/// Write raw bytes to the current sink (stderr by default) with EINTR retry.
void write_raw(const char *data, size_t size);

/// Write a string_view to the current sink.
void write_str(std::string_view value);

/// Write the formatted log prefix to the current sink.
/// NOT mutex-protected — use write_log_line() for atomic output.
void write_prefix(Level level);

/// Write a decimal number (stack-allocated, no heap).
void write_dec(size_t value);

/// Write a hex number with "0x" prefix (stack-allocated, no heap).
void write_hex(uintptr_t value);

// #######################################
//  System info
// #######################################

/// Return the cached process ID.
[[nodiscard]] int pid();

/// Return the current thread ID (platform-specific).
[[nodiscard]] unsigned long long thread_id();

// #######################################
//  Internal — used by log() templates
// #######################################

/// Write a complete log line atomically (prefix + message).
/// If module_name is non-empty, it is included in the prefix.
/// Protected by the log mutex when thread safety is enabled.
void write_log_line(Level level, std::string_view module_name,
                    std::string_view message, const std::source_location &loc);

/// Lazy one-time initialization (env vars, etc.).
void init_once();

// #######################################
//  Main logging function
// #######################################

/// Log a formatted message at the given level.
/// Uses std::format syntax. Only writes when logging is enabled and the
/// level passes the minimum filter.
///
/// Example:
///   coretrace::log(Level::Info, "Hello {}\n", "world");
///   coretrace::log(Level::Warn, "count={}\n", 42);
///
template <typename... Args>
inline void log(LogEntry entry, std::string_view fmt, Args &&...args) {
  init_once();

  if (!log_is_enabled())
    return;
  if (static_cast<int>(entry.level) < static_cast<int>(min_level()))
    return;

  try {
    std::string msg = std::vformat(fmt, std::make_format_args(args...));
    if (msg.empty())
      return;

    write_log_line(entry.level, {}, msg, entry.loc);
  } catch (...) {
    static const char fallback[] = "coretrace: log format error\n";
    write_raw(fallback, sizeof(fallback) - 1);
  }
}

/// Log a formatted message with a module tag.
/// The message is only emitted if the module filter passes.
///
/// Example:
///   coretrace::log(Level::Info, Module("alloc"), "malloc ptr={:p}\n", ptr);
///
template <typename... Args>
inline void log(LogEntry entry, Module mod, std::string_view fmt,
                Args &&...args) {
  init_once();

  if (!log_is_enabled())
    return;
  if (static_cast<int>(entry.level) < static_cast<int>(min_level()))
    return;
  if (!mod.name.empty() && !module_is_enabled(mod.name))
    return;

  try {
    std::string msg = std::vformat(fmt, std::make_format_args(args...));
    if (msg.empty())
      return;

    write_log_line(entry.level, mod.name, msg, entry.loc);
  } catch (...) {
    static const char fallback[] = "coretrace: log format error\n";
    write_raw(fallback, sizeof(fallback) - 1);
  }
}

} // namespace coretrace

#endif // CORETRACE_LOGGER_HPP
