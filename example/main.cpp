#include <coretrace/logger.hpp>

#include <cstdio>
#include <string>

// Custom sink: writes to a string buffer (for demo).
static std::string g_buffer;
static void buffer_sink(const char *data, size_t size) {
  g_buffer.append(data, size);
}

int main() {
  using namespace coretrace;

  enable_logging();

  // ── 1. Basic logging ────────────────────────
  log(Level::Info, "Logger initialized\n");
  log(Level::Warn, "This is a warning: value={}\n", 42);
  log(Level::Error, "Something went wrong!\n");

  // ── 2. Level filtering ──────────────────────
  set_min_level(Level::Warn);
  log(Level::Info, "This INFO should NOT appear\n");
  log(Level::Warn, "This WARN should appear\n");
  log(Level::Error, "This ERROR should appear\n");
  set_min_level(Level::Info); // reset

  // ── 3. Module filtering ─────────────────────
  enable_module("alloc");
  enable_module("trace");

  log(Level::Info, Module("alloc"), "malloc ptr=0x{:x} size={}\n", 0xDEADBEEF,
      64);
  log(Level::Info, Module("trace"), "enter main()\n");
  log(Level::Info, Module("network"),
      "This should NOT appear (module not enabled)\n");

  enable_all_modules(); // reset

  // ── 4. Timestamps ───────────────────────────
  set_timestamps(true);
  log(Level::Info, "This line has a timestamp\n");
  set_timestamps(false);

  // ── 5. Source location ──────────────────────
  set_source_location(true);
  log(Level::Info, "This line shows file:line\n");
  set_source_location(false);

  // ── 6. All features combined ────────────────
  set_prefix("==myapp==");
  set_timestamps(true);
  set_source_location(true);
  enable_module("db");

  log(Level::Warn, Module("db"), "Connection pool exhausted, count={}\n", 0);

  set_timestamps(false);
  set_source_location(false);
  enable_all_modules();
  set_prefix("==ct==");

  // ── 7. Custom sink ──────────────────────────
  set_sink(buffer_sink);
  log(Level::Info, "This goes to the buffer, not stderr\n");
  reset_sink();

  std::fprintf(stderr, "\n--- Buffer sink captured %zu bytes ---\n",
               g_buffer.size());
  std::fwrite(g_buffer.data(), 1, g_buffer.size(), stderr);

  // ── 8. Disable logging ──────────────────────
  disable_logging();
  log(Level::Error, "This should NOT appear\n");

  return 0;
}
