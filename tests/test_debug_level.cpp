#include <coretrace/logger.hpp>

#include <cstdlib>
#include <string>

namespace {

std::string g_capture;

void capture_sink(const char *data, size_t size) { g_capture.append(data, size); }

} // namespace

int main() {
  using namespace coretrace;

  setenv("CT_LOG_LEVEL", "debug", 1);

  set_sink(capture_sink);
  enable_logging();

  // Env defaults should allow DEBUG before explicit API override.
  log(Level::Debug, "debug via env\n");

  set_min_level(Level::Info);
  log(Level::Debug, "debug filtered by info\n");
  log(Level::Info, "info still visible\n");

  reset_sink();
  unsetenv("CT_LOG_LEVEL");

  const bool has_debug_env = g_capture.find("debug via env") != std::string::npos;
  const bool has_debug_filtered =
      g_capture.find("debug filtered by info") != std::string::npos;
  const bool has_info = g_capture.find("info still visible") != std::string::npos;

  if (!has_debug_env || has_debug_filtered || !has_info)
    return 1;

  return 0;
}
