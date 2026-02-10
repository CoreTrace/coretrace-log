#include "coretrace/logger.hpp"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#endif

namespace coretrace
{

// ####################################
//  Global state
// ####################################

namespace
{
    // ── Enable / Disable ─────────────────────

    int g_log_enabled = 0;

    // ── Prefix ───────────────────────────────

    char g_prefix_buf[64] = "==ct==";
    size_t g_prefix_len = 6;

    // ── Level filtering ──────────────────────

    int g_min_level = 0; // Level::Info
    int g_min_level_set_explicitly = 0;

    // ── Module filtering ─────────────────────

    constexpr int MAX_MODULES = 32;
    constexpr int MODULE_NAME_LEN = 32;

    struct ModuleTable
    {
        char names[MAX_MODULES][MODULE_NAME_LEN];
        int count = 0;
        int filter_active = 0; // 1 if at least one module was registered
    };

    ModuleTable g_modules{};
    int g_modules_set_explicitly = 0;

    // ── Synchronization ──────────────────────

    // Protects mutable logger state (prefix + modules table).
    pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Protects atomicity of one log line output when thread-safe mode is on.
    pthread_mutex_t g_output_mutex = PTHREAD_MUTEX_INITIALIZER;
    int g_thread_safe = 1; // enabled by default

    // ── Sink ─────────────────────────────────

    SinkFn g_sink = nullptr;

    // ── Timestamps ───────────────────────────

    int g_timestamps_enabled = 0;

    // ── Source location ──────────────────────

    int g_source_location_enabled = 0;

    // ── Init ─────────────────────────────────

    pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

    // ── Small lock guards ────────────────────

    struct StateLockGuard
    {
        StateLockGuard() { pthread_mutex_lock(&g_state_mutex); }
        ~StateLockGuard() { pthread_mutex_unlock(&g_state_mutex); }

        StateLockGuard(const StateLockGuard&) = delete;
        StateLockGuard& operator=(const StateLockGuard&) = delete;
    };

    struct OutputLockGuard
    {
        OutputLockGuard()
            : locked(__atomic_load_n(&g_thread_safe, __ATOMIC_ACQUIRE) != 0)
        {
            if (locked)
                pthread_mutex_lock(&g_output_mutex);
        }

        ~OutputLockGuard()
        {
            if (locked)
                pthread_mutex_unlock(&g_output_mutex);
        }

        OutputLockGuard(const OutputLockGuard&) = delete;
        OutputLockGuard& operator=(const OutputLockGuard&) = delete;

        bool locked;
    };

    struct PrefixSnapshot
    {
        char value[sizeof(g_prefix_buf)];
        size_t len = 0;
    };

    [[nodiscard]] PrefixSnapshot read_prefix_snapshot()
    {
        PrefixSnapshot snapshot{};

        StateLockGuard guard;

        snapshot.len = g_prefix_len;
        if (snapshot.len > sizeof(snapshot.value))
            snapshot.len = sizeof(snapshot.value);

        std::memcpy(snapshot.value, g_prefix_buf, snapshot.len);
        return snapshot;
    }

    // ── Color detection ──────────────────────

    [[nodiscard]] bool use_color()
    {
        static int cached = -1;

        if (cached != -1)
            return cached != 0;

        if (getenv("NO_COLOR") != nullptr)
        {
            cached = 0;
            return false;
        }

        cached = isatty(2) ? 1 : 0;
        return cached != 0;
    }

    // ── String helpers ───────────────────────

    [[nodiscard]] bool sv_eq(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i] != b[i])
                return false;
        }
        return true;
    }

    [[nodiscard]] bool cstr_ieq(const char* a, const char* b)
    {
        while (*a && *b)
        {
            char ca = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
            char cb = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
            if (ca != cb)
                return false;
            ++a;
            ++b;
        }
        return *a == *b;
    }

    // ── Timestamp formatting ─────────────────

    // Writes ISO 8601 timestamp: [2025-01-15T10:45:23.456]
    // Uses stack buffer, no heap allocation.
    void write_timestamp_to(char* buf, size_t& idx)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        struct tm tm_buf;
        gmtime_r(&ts.tv_sec, &tm_buf);

        int millis = static_cast<int>(ts.tv_nsec / 1000000);

        // [YYYY-MM-DDThh:mm:ss.mmm]
        buf[idx++] = '[';

        // Year
        int year = tm_buf.tm_year + 1900;
        buf[idx++] = static_cast<char>('0' + (year / 1000));
        buf[idx++] = static_cast<char>('0' + (year / 100) % 10);
        buf[idx++] = static_cast<char>('0' + (year / 10) % 10);
        buf[idx++] = static_cast<char>('0' + year % 10);
        buf[idx++] = '-';

        // Month
        int mon = tm_buf.tm_mon + 1;
        buf[idx++] = static_cast<char>('0' + mon / 10);
        buf[idx++] = static_cast<char>('0' + mon % 10);
        buf[idx++] = '-';

        // Day
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_mday / 10);
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_mday % 10);
        buf[idx++] = 'T';

        // Hour
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_hour / 10);
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_hour % 10);
        buf[idx++] = ':';

        // Minute
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_min / 10);
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_min % 10);
        buf[idx++] = ':';

        // Second
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_sec / 10);
        buf[idx++] = static_cast<char>('0' + tm_buf.tm_sec % 10);
        buf[idx++] = '.';

        // Milliseconds
        buf[idx++] = static_cast<char>('0' + millis / 100);
        buf[idx++] = static_cast<char>('0' + (millis / 10) % 10);
        buf[idx++] = static_cast<char>('0' + millis % 10);

        buf[idx++] = ']';
        buf[idx++] = ' ';
    }

    // ── Extract basename from path ───────────

    [[nodiscard]] const char* basename_of(const char* path)
    {
        if (!path)
            return "<unknown>";

        const char* last = path;
        for (const char* p = path; *p; ++p)
        {
            if (*p == '/')
                last = p + 1;
        }
        return last;
    }

    // ── Module helpers (state lock required) ─

    void add_module_locked(std::string_view name)
    {
        // Check if already registered.
        for (int i = 0; i < g_modules.count; ++i)
        {
            if (sv_eq(name, std::string_view(g_modules.names[i])))
                return;
        }

        if (g_modules.count < MAX_MODULES)
        {
            for (size_t i = 0; i < name.size(); ++i)
                g_modules.names[g_modules.count][i] = name[i];
            g_modules.names[g_modules.count][name.size()] = '\0';
            g_modules.count++;
            g_modules.filter_active = 1;
        }
    }

    void init_from_env()
    {
        // CT_LOG_LEVEL=info|warn|error (default only, explicit API has priority)
        if (__atomic_load_n(&g_min_level_set_explicitly, __ATOMIC_ACQUIRE) == 0)
        {
            const char* env_level = getenv("CT_LOG_LEVEL");
            if (env_level)
            {
                if (cstr_ieq(env_level, "warn"))
                    __atomic_store_n(&g_min_level, 1, __ATOMIC_RELEASE);
                else if (cstr_ieq(env_level, "error"))
                    __atomic_store_n(&g_min_level, 2, __ATOMIC_RELEASE);
                else // "info" or anything else
                    __atomic_store_n(&g_min_level, 0, __ATOMIC_RELEASE);
            }
        }

        // CT_DEBUG=mod1,mod2,... (default only, explicit API has priority)
        if (__atomic_load_n(&g_modules_set_explicitly, __ATOMIC_ACQUIRE) == 0)
        {
            const char* env_debug = getenv("CT_DEBUG");
            if (env_debug && env_debug[0] != '\0')
            {
                StateLockGuard guard;

                // Parse comma-separated module names.
                const char* start = env_debug;
                while (*start)
                {
                    const char* end = start;
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

void init_once()
{
    (void)pthread_once(&g_init_once, init_from_env);
}

// ####################################
//  Enable / Disable
// ####################################

void enable_logging()
{
    __atomic_store_n(&g_log_enabled, 1, __ATOMIC_RELEASE);
}

void disable_logging()
{
    __atomic_store_n(&g_log_enabled, 0, __ATOMIC_RELEASE);
}

[[nodiscard]] bool log_is_enabled()
{
    return __atomic_load_n(&g_log_enabled, __ATOMIC_ACQUIRE) != 0;
}

// ####################################
//  Prefix
// ####################################

void set_prefix(std::string_view prefix)
{
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

void set_min_level(Level level)
{
    __atomic_store_n(&g_min_level_set_explicitly, 1, __ATOMIC_RELEASE);
    init_once();
    __atomic_store_n(&g_min_level, static_cast<int>(level), __ATOMIC_RELEASE);
}

[[nodiscard]] Level min_level()
{
    return static_cast<Level>(__atomic_load_n(&g_min_level, __ATOMIC_ACQUIRE));
}

// ####################################
//  Module filtering
// ####################################

void enable_module(std::string_view name)
{
    if (name.empty() || name.size() >= MODULE_NAME_LEN)
        return;

    __atomic_store_n(&g_modules_set_explicitly, 1, __ATOMIC_RELEASE);
    init_once();

    StateLockGuard guard;
    add_module_locked(name);
}

void disable_module(std::string_view name)
{
    if (name.empty())
        return;

    __atomic_store_n(&g_modules_set_explicitly, 1, __ATOMIC_RELEASE);
    init_once();

    StateLockGuard guard;

    for (int i = 0; i < g_modules.count; ++i)
    {
        if (sv_eq(name, std::string_view(g_modules.names[i])))
        {
            // Shift remaining entries.
            for (int j = i; j < g_modules.count - 1; ++j)
                std::memcpy(g_modules.names[j], g_modules.names[j + 1], MODULE_NAME_LEN);
            g_modules.count--;

            if (g_modules.count == 0)
                g_modules.filter_active = 0;

            break;
        }
    }
}

void enable_all_modules()
{
    __atomic_store_n(&g_modules_set_explicitly, 1, __ATOMIC_RELEASE);
    init_once();

    StateLockGuard guard;
    g_modules.count = 0;
    g_modules.filter_active = 0;
}

[[nodiscard]] bool module_is_enabled(std::string_view name)
{
    StateLockGuard guard;

    // If no filter is active, everything passes.
    if (!g_modules.filter_active)
        return true;

    for (int i = 0; i < g_modules.count; ++i)
    {
        if (sv_eq(name, std::string_view(g_modules.names[i])))
            return true;
    }

    return false;
}

// ####################################
//  Thread safety
// ####################################

void set_thread_safe(bool enabled)
{
    __atomic_store_n(&g_thread_safe, enabled ? 1 : 0, __ATOMIC_RELEASE);
}

// ####################################
//  Sink
// ####################################

void set_sink(SinkFn fn)
{
    __atomic_store_n(&g_sink, fn, __ATOMIC_RELEASE);
}

void reset_sink()
{
    __atomic_store_n(&g_sink, static_cast<SinkFn>(nullptr), __ATOMIC_RELEASE);
}

// ####################################
//  Timestamps
// ####################################

void set_timestamps(bool enabled)
{
    __atomic_store_n(&g_timestamps_enabled, enabled ? 1 : 0, __ATOMIC_RELEASE);
}

// ####################################
//  Source location
// ####################################

void set_source_location(bool enabled)
{
    __atomic_store_n(&g_source_location_enabled, enabled ? 1 : 0, __ATOMIC_RELEASE);
}

// ####################################
//  Color
// ####################################

[[nodiscard]] std::string_view color(Color c)
{
    if (!use_color())
        return {};

    switch (c)
    {
    case Color::Reset:          return "\x1b[0m";

    case Color::Dim:            return "\x1b[2m";
    case Color::Bold:           return "\x1b[1m";
    case Color::Underline:      return "\x1b[4m";
    case Color::Italic:         return "\x1b[3m";
    case Color::Blink:          return "\x1b[5m";
    case Color::Reverse:        return "\x1b[7m";
    case Color::Hidden:         return "\x1b[8m";
    case Color::Strike:         return "\x1b[9m";

    case Color::Black:          return "\x1b[30m";
    case Color::Red:            return "\x1b[31m";
    case Color::Green:          return "\x1b[32m";
    case Color::Yellow:         return "\x1b[33m";
    case Color::Blue:           return "\x1b[34m";
    case Color::Magenta:        return "\x1b[35m";
    case Color::Cyan:           return "\x1b[36m";
    case Color::White:          return "\x1b[37m";

    case Color::Gray:           return "\x1b[90m";
    case Color::BrightRed:      return "\x1b[91m";
    case Color::BrightGreen:    return "\x1b[92m";
    case Color::BrightYellow:   return "\x1b[93m";
    case Color::BrightBlue:     return "\x1b[94m";
    case Color::BrightMagenta:  return "\x1b[95m";
    case Color::BrightCyan:     return "\x1b[96m";
    case Color::BrightWhite:    return "\x1b[97m";

    case Color::BgBlack:        return "\x1b[40m";
    case Color::BgRed:          return "\x1b[41m";
    case Color::BgGreen:        return "\x1b[42m";
    case Color::BgYellow:       return "\x1b[43m";
    case Color::BgBlue:         return "\x1b[44m";
    case Color::BgMagenta:      return "\x1b[45m";
    case Color::BgCyan:         return "\x1b[46m";
    case Color::BgWhite:        return "\x1b[47m";

    case Color::BgGray:           return "\x1b[100m";
    case Color::BgBrightRed:      return "\x1b[101m";
    case Color::BgBrightGreen:    return "\x1b[102m";
    case Color::BgBrightYellow:   return "\x1b[103m";
    case Color::BgBrightBlue:     return "\x1b[104m";
    case Color::BgBrightMagenta:  return "\x1b[105m";
    case Color::BgBrightCyan:     return "\x1b[106m";
    case Color::BgBrightWhite:    return "\x1b[107m";
    }

    return {};
}

[[nodiscard]] std::string_view level_label(Level level)
{
    switch (level)
    {
    case Level::Info:  return "INFO";
    case Level::Warn:  return "WARN";
    case Level::Error: return "ERROR";
    }
    return "INFO";
}

[[nodiscard]] std::string_view level_color(Level level)
{
    switch (level)
    {
    case Level::Info:  return color(Color::Green);
    case Level::Warn:  return color(Color::Yellow);
    case Level::Error: return color(Color::Red);
    }
    return color(Color::Cyan);
}

// ####################################
//  System info
// ####################################

[[nodiscard]] int pid()
{
    static int cached = 0;

    if (cached == 0)
        cached = static_cast<int>(getpid());

    return cached;
}

[[nodiscard]] unsigned long long thread_id()
{
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

// ####################################
//  Low-level write
// ####################################

void write_raw(const char* data, size_t size)
{
    if (!data || size == 0)
        return;

    // Custom sink?
    SinkFn sink = __atomic_load_n(&g_sink, __ATOMIC_ACQUIRE);
    if (sink)
    {
        sink(data, size);
        return;
    }

    // Default: stderr (fd=2) with EINTR retry.
    while (size > 0)
    {
        ssize_t written = write(2, data, size);
        if (written > 0)
        {
            data += static_cast<size_t>(written);
            size -= static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        break;
    }
}

void write_str(std::string_view value)
{
    if (value.empty())
        return;
    write_raw(value.data(), value.size());
}

void write_dec(size_t value)
{
    char buf[32];
    size_t idx = 0;

    if (value == 0)
    {
        buf[idx++] = '0';
    }
    else
    {
        while (value != 0 && idx < sizeof(buf))
        {
            buf[idx++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
    }

    // Reverse digits.
    for (size_t i = 0; i < idx / 2; ++i)
    {
        char tmp = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = tmp;
    }

    write_raw(buf, idx);
}

void write_hex(uintptr_t value)
{
    char buf[2 + sizeof(uintptr_t) * 2];
    size_t idx = 0;

    buf[idx++] = '0';
    buf[idx++] = 'x';

    bool started = false;
    for (int shift = static_cast<int>(sizeof(uintptr_t) * 8 - 4); shift >= 0; shift -= 4)
    {
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

void write_prefix(Level level)
{
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
                    std::string_view message, const std::source_location& loc)
{
    PrefixSnapshot prefix = read_prefix_snapshot();
    OutputLockGuard output_lock;

    // Optional timestamp: [2025-01-15T10:45:23.456]
    if (__atomic_load_n(&g_timestamps_enabled, __ATOMIC_ACQUIRE))
    {
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
    if (__atomic_load_n(&g_source_location_enabled, __ATOMIC_ACQUIRE))
    {
        write_raw(" ", 1);
        write_str(color(Color::Dim));
        const char* file = basename_of(loc.file_name());
        write_raw(file, std::strlen(file));
        write_raw(":", 1);
        write_dec(static_cast<size_t>(loc.line()));
        write_str(color(Color::Reset));
    }

    // Optional module tag: (alloc)
    if (!module.empty())
    {
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
