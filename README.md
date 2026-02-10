# coretrace-logger

A minimal, fast, and thread-safe C++20 logging library with colored terminal output.

- Zero dependencies (no Boost, no fmt, no spdlog)
- `std::format`-based type-safe formatting
- Atomic enable/disable, mutex-protected output (no interleaved lines)
- ANSI color support with automatic detection and `NO_COLOR` compliance
- Level filtering, module filtering, timestamps, source location
- Custom sink support (redirect to file, buffer, syslog, etc.)
- ~700 lines total

## Quick start

```cpp
#include <coretrace/logger.hpp>

int main()
{
    coretrace::enable_logging();
    coretrace::log(coretrace::Level::Info, "Hello {}\n", "world");
    coretrace::log(coretrace::Level::Warn, "count={}\n", 42);
    coretrace::log(coretrace::Level::Error, "disk full\n");
}
```

Output:
```
|12345| ==ct== [INFO] Hello world
|12345| ==ct== [WARN] count=42
|12345| ==ct== [ERROR] disk full
```

## Integration

### Option 1 : FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(coretrace-logger
  GIT_REPOSITORY https://github.com/<your-org>/coretrace-logger.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(coretrace-logger)

target_link_libraries(my_target PRIVATE coretrace::logger)
```

### Option 2 : add_subdirectory (local)

```cmake
add_subdirectory(path/to/coretrace-logger)
target_link_libraries(my_target PRIVATE coretrace::logger)
```

### Option 3 : install + find_package

```bash
cd coretrace-logger && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j && make install
```

```cmake
find_package(coretrace-logger REQUIRED)
target_link_libraries(my_target PRIVATE coretrace::logger)
```

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `CORETRACE_LOGGER_BUILD_EXAMPLES` | `ON` | Build the example program |
| `CORETRACE_LOGGER_BUILD_TESTS` | `ON` (top-level) | Build and register CTest tests |

> When consumed via `FetchContent` or `add_subdirectory`, set the option to `OFF` before the include to skip building examples:
> ```cmake
> set(CORETRACE_LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
> ```

## Requirements

- C++20 compiler (Clang 16+, GCC 13+, AppleClang 15+)
- POSIX (Linux, macOS) for `write(2)`, `pthread`, `clock_gettime`

## API reference

### Core

```cpp
coretrace::enable_logging();            // Enable output (disabled by default)
coretrace::disable_logging();           // Disable output
coretrace::log_is_enabled();            // Check state

coretrace::set_prefix("==myapp==");     // Change the log tag (default: "==ct==")
```

### Logging

```cpp
// Basic
coretrace::log(Level::Info, "message {}\n", value);

// With module tag
coretrace::log(Level::Info, Module("alloc"), "malloc size={}\n", 64);
```

Uses `std::format` syntax. The `Level` is implicitly converted to a `LogEntry` that captures `std::source_location` at the call site.

### Level filtering

```cpp
coretrace::set_min_level(Level::Warn);  // Only Warn + Error pass
coretrace::set_min_level(Level::Info);  // Everything passes (default)
```

Or via environment variable:
```bash
CT_LOG_LEVEL=error ./my_program    # Only errors
CT_LOG_LEVEL=warn  ./my_program    # Warn + Error
```

`CT_LOG_LEVEL` sets a startup default. Explicit calls to `set_min_level()` always take precedence.

### Module filtering

```cpp
coretrace::enable_module("alloc");
coretrace::enable_module("trace");

coretrace::log(Level::Info, Module("alloc"), "tracked\n");    // printed
coretrace::log(Level::Info, Module("network"), "dropped\n");  // filtered out
coretrace::log(Level::Info, "no module = always printed\n");  // printed

coretrace::enable_all_modules();  // Clear filter, everything passes
```

Or via environment variable:
```bash
CT_DEBUG=alloc,trace ./my_program
```

`CT_DEBUG` sets a startup default. Explicit module API calls always take precedence.

### Timestamps

```cpp
coretrace::set_timestamps(true);
```

Output:
```
[2025-01-15T10:45:23.456] |12345| ==ct== [INFO] message
```

### Source location

```cpp
coretrace::set_source_location(true);
```

Output:
```
|12345| ==ct== [INFO] main.cpp:42 message
```

### Custom sink

```cpp
// Redirect to a file
FILE* f = fopen("app.log", "w");
coretrace::set_sink([](const char* data, size_t size) {
    fwrite(data, 1, size, f);
});

// Back to stderr
coretrace::reset_sink();
```

### Thread safety

```cpp
coretrace::set_thread_safe(true);   // Default: mutex-protected output
coretrace::set_thread_safe(false);  // Disable for single-threaded hot paths
```

### Colors

```cpp
coretrace::log(Level::Info, "{}bold{} and {}red{}\n",
    coretrace::color(Color::Bold),
    coretrace::color(Color::Reset),
    coretrace::color(Color::Red),
    coretrace::color(Color::Reset));
```

Colors are automatically disabled when stderr is not a terminal or when `NO_COLOR` is set.

### Low-level API

```cpp
coretrace::write_prefix(Level::Info);      // Write formatted prefix
coretrace::write_str("hello");             // Write string
coretrace::write_dec(42);                  // Write decimal (no heap)
coretrace::write_hex(0xDEAD);             // Write hex with 0x prefix
coretrace::write_raw(buf, len);            // Write raw bytes
coretrace::pid();                          // Cached PID
coretrace::thread_id();                    // Platform-specific TID
```

## Environment variables

| Variable | Values | Description |
|----------|--------|-------------|
| `CT_LOG_LEVEL` | `info`, `warn`, `error` | Set startup default minimum log level |
| `CT_DEBUG` | comma-separated names | Set startup default enabled modules |
| `NO_COLOR` | any value | Disable ANSI color output |

## Output format

```
[timestamp] |PID| ==prefix== [LEVEL] file:line (module) message
```

Each field is optional:
- **timestamp** : enabled via `set_timestamps(true)`
- **PID** : always shown
- **prefix** : configurable via `set_prefix()`
- **LEVEL** : `INFO` (green), `WARN` (yellow), `ERROR` (red)
- **file:line** : enabled via `set_source_location(true)`
- **module** : shown when using the `Module()` overload

## License

MIT
